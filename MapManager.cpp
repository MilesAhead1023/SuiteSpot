#include "pch.h"
#include "MapManager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_set>

#include "IMGUI/json.hpp"

namespace {
std::string Trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string StripQuotes(const std::string& value)
{
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string ExpandEnvAndHome(const std::string& input)
{
    std::string expanded;
    expanded.reserve(input.size());

    for (size_t i = 0; i < input.size();) {
        if (input[i] == '%') {
            const auto end = input.find('%', i + 1);
            if (end != std::string::npos) {
                const auto varName = input.substr(i + 1, end - i - 1);
                if (!varName.empty()) {
                    if (const char* val = std::getenv(varName.c_str())) {
                        expanded.append(val);
                    }
                }
                i = end + 1;
                continue;
            }
        }
        expanded.push_back(input[i]);
        ++i;
    }

    if (!expanded.empty() && expanded[0] == '~') {
        if (const char* home = std::getenv("USERPROFILE")) {
            expanded.replace(0, 1, home);
        }
    }

    return expanded;
}

int CaseInsensitiveCompare(const std::string& a, const std::string& b)
{
    const size_t len = std::min(a.size(), b.size());
    for (size_t i = 0; i < len; i++) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) return (ca < cb) ? -1 : 1;
    }
    if (a.size() == b.size()) return 0;
    return (a.size() < b.size()) ? -1 : 1;
}

} // namespace

MapManager::MapManager() {}

std::filesystem::path MapManager::GetDataRoot() const
{
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return std::filesystem::path();
    return std::filesystem::path(appdata) / "bakkesmod" / "bakkesmod" / "data";
}

std::filesystem::path MapManager::GetSuiteSpotRoot() const
{
    return GetDataRoot() / "SuiteSpot";
}

std::filesystem::path MapManager::GetSuiteTrainingDir() const
{
    return GetSuiteSpotRoot() / "TrainingSuite";
}

std::filesystem::path MapManager::GetWorkshopCacheDir() const
{
    return GetSuiteSpotRoot() / "Workshop";
}

std::filesystem::path MapManager::GetTexturesDir() const
{
    return GetWorkshopCacheDir(); // Same as workshop cache
}

std::filesystem::path MapManager::GetTrainingPacksPath() const
{
    return GetSuiteTrainingDir() / "training_packs.json";
}

std::filesystem::path MapManager::GetWorkshopLoaderConfigPath() const
{
    return GetDataRoot() / "WorkshopMapLoader" / "workshopmaploader.cfg";
}

std::filesystem::path MapManager::ResolveConfiguredWorkshopRoot() const
{
    const auto cfg = GetWorkshopLoaderConfigPath();
    std::ifstream in(cfg);
    if (!in.is_open()) {
        return {};
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const auto keyPos = trimmed.find("MapsFolderPath");
        if (keyPos == std::string::npos) {
            continue;
        }

        const auto eqPos = trimmed.find('=', keyPos);
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string value = trimmed.substr(eqPos + 1);
        value = StripQuotes(Trim(value));
        value = ExpandEnvAndHome(value);

        if (value.empty()) {
            continue;
        }

        std::error_code ec;
        std::filesystem::path candidate(value);
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
            return candidate;
        }

        LOG("SuiteSpot: Configured workshop path not found: " + value);
    }

    return {};
}

void MapManager::EnsureDataDirectories() const
{
    std::error_code ec;
    auto root = GetDataRoot();
    if (!root.empty()) std::filesystem::create_directories(root, ec);
    ec.clear();
    std::filesystem::create_directories(GetSuiteTrainingDir(), ec);
}

bool MapManager::LoadWorkshopMetadata(const std::filesystem::path& jsonPath, std::string& outTitle,
                                      std::string& outAuthor, std::string& outDescription) const
{
    std::error_code ec;
    if (!std::filesystem::exists(jsonPath, ec)) return false;

    try {
        std::ifstream file(jsonPath);
        if (!file.is_open()) return false;

        // Read entire file content
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Sanitize content: replace control characters that break JSON parsing
        for (char& c : content) {
            if (c >= 0 && c < 32 && c != '\t' && c != '\n' && c != '\r') {
                c = ' '; // Replace control chars with space
            }
        }

        // Try parsing with allow_exceptions=false first
        nlohmann::json j = nlohmann::json::parse(content, nullptr, false);

        if (j.is_discarded()) {
            // JSON parsing failed - try to extract fields manually as fallback
            // Look for "Title":"value" pattern
            auto extractField = [&content](const std::string& fieldName) -> std::string {
                std::string pattern = "\"" + fieldName + "\":\"";
                size_t start = content.find(pattern);
                if (start == std::string::npos) return "";
                start += pattern.length();
                size_t end = content.find("\"", start);
                if (end == std::string::npos) return "";
                return content.substr(start, end - start);
            };

            outTitle = extractField("Title");
            outAuthor = extractField("Author");
            // Skip description for malformed files - it's usually what contains the bad data
            outDescription = "";

            return !outTitle.empty(); // Consider success if we at least got the title
        }

        if (j.contains("Title") && j["Title"].is_string()) {
            outTitle = j["Title"].get<std::string>();
        }
        if (j.contains("Author") && j["Author"].is_string()) {
            outAuthor = j["Author"].get<std::string>();
        }
        if (j.contains("Description") && j["Description"].is_string()) {
            outDescription = j["Description"].get<std::string>();
        }
        return true;
    } catch (const std::exception&) {
        // Silently fail for malformed JSON - don't spam the log
        return false;
    }
}

std::filesystem::path MapManager::FindPreviewImage(const std::filesystem::path& folder) const
{
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec)) return {};

    // Check common preview image extensions
    static const std::vector<std::string> extensions = {".jfif", ".jpg", ".jpeg", ".png", ".gif", ".webp"};

    for (const auto& file : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!file.is_regular_file()) continue;

        std::string ext = file.path().extension().string();
        // Convert to lowercase for comparison
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

        for (const auto& validExt : extensions) {
            if (ext == validExt) {
                return file.path();
            }
        }
    }
    return {};
}

void MapManager::DiscoverWorkshopInDir(const std::filesystem::path& dir, std::vector<WorkshopEntry>& workshop) const
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_directory()) continue;

        std::string foundMapFile;
        std::filesystem::path foundJsonFile;

        // Scan for UPK and JSON files
        for (const auto& file : std::filesystem::directory_iterator(entry.path(), ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!file.is_regular_file()) continue;

            const auto& path = file.path();
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

            if (ext == ".upk" && foundMapFile.empty()) {
                foundMapFile = path.string();
            } else if (ext == ".json" && foundJsonFile.empty()) {
                foundJsonFile = path;
            }
        }

        if (!foundMapFile.empty()) {
            WorkshopEntry workshopEntry;
            workshopEntry.filePath = foundMapFile;
            workshopEntry.folder = entry.path();
            workshopEntry.name = entry.path().filename().string();

            // Try to load metadata from JSON
            std::string title, author, description;
            if (!foundJsonFile.empty() && LoadWorkshopMetadata(foundJsonFile, title, author, description)) {
                if (!title.empty()) workshopEntry.name = title;
                workshopEntry.author = author;
                workshopEntry.description = description;
            }

            // Find preview image
            workshopEntry.previewPath = FindPreviewImage(entry.path());

            workshop.push_back(workshopEntry);
        }
    }
}

void MapManager::LoadWorkshopMaps(std::vector<WorkshopEntry>& workshop, int& currentWorkshopIndex)
{
    workshop.clear();

    std::vector<std::filesystem::path> roots;
    std::unordered_set<std::string> seenRoots;

    // Helper to add root only if not already seen
    auto addRoot = [&](const std::filesystem::path& path) {
        if (path.empty()) return;
        std::error_code ec;
        std::string canonical;
        if (std::filesystem::exists(path, ec)) {
            canonical = std::filesystem::canonical(path, ec).string();
        }
        if (canonical.empty()) {
            canonical = path.string();
        }
        // Normalize to lowercase for comparison on Windows
        std::string lower = canonical;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (seenRoots.insert(lower).second) {
            roots.push_back(path);
        }
    };

    if (const auto configured = ResolveConfiguredWorkshopRoot(); !configured.empty()) {
        addRoot(configured);
    }

    const char* progFiles = std::getenv("ProgramFiles");
    const char* progFilesX86 = std::getenv("ProgramFiles(x86)");

    if (progFiles) {
        addRoot(std::filesystem::path(progFiles) / "Epic Games" / "rocketleague" / "TAGame" / "CookedPCConsole" / "mods");
    }
    if (progFilesX86) {
        addRoot(std::filesystem::path(progFilesX86) / "Steam" / "steamapps" / "common" / "rocketleague" / "TAGame" /
                "CookedPCConsole" / "mods");
    }

    for (const auto& root : roots) {
        DiscoverWorkshopInDir(root, workshop);
    }

    std::unordered_set<std::string> seen;
    std::vector<WorkshopEntry> unique;
    unique.reserve(workshop.size());
    for (const auto& entry : workshop) {
        if (seen.insert(entry.filePath).second) {
            unique.push_back(entry);
        }
    }
    workshop.swap(unique);

    std::sort(workshop.begin(), workshop.end(), [](const WorkshopEntry& lhs, const WorkshopEntry& rhs) {
        const int cmp = CaseInsensitiveCompare(lhs.name, rhs.name);
        if (cmp == 0) {
            return lhs.filePath < rhs.filePath;
        }
        return cmp < 0;
    });

    if (workshop.empty()) {
        currentWorkshopIndex = 0;
    } else {
        currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, static_cast<int>(workshop.size() - 1));
    }
}

// ===== HOTKEY CYCLING METHODS =====

void MapManager::CycleMapMode(bool forward)
{
    // Cycle through map modes: Freeplay (0) → Training (1) → Workshop (2) → Freeplay (0)
    if (forward) {
        currentMapModeIndex = (currentMapModeIndex + 1) % 3;
    } else {
        currentMapModeIndex = (currentMapModeIndex - 1 + 3) % 3;
    }
    // Caller (SuiteSpot hotkey handler) will update the CVar with currentMapModeIndex
}

void MapManager::CycleMap(bool forward)
{
    // Cycle through maps within the current mode
    // Get the current list based on current map mode
    if (currentMapModeIndex == 0 && !RLMaps.empty()) {
        // Freeplay maps
        if (forward) {
            currentFreeplayIndex = (currentFreeplayIndex + 1) % RLMaps.size();
        } else {
            currentFreeplayIndex = (currentFreeplayIndex - 1 + RLMaps.size()) % RLMaps.size();
        }
    } else if (currentMapModeIndex == 1 && !RLTraining.empty()) {
        // Training packs
        if (forward) {
            currentTrainingIndex = (currentTrainingIndex + 1) % RLTraining.size();
        } else {
            currentTrainingIndex = (currentTrainingIndex - 1 + RLTraining.size()) % RLTraining.size();
        }
    } else if (currentMapModeIndex == 2 && !RLWorkshop.empty()) {
        // Workshop maps
        if (forward) {
            currentWorkshopIndex = (currentWorkshopIndex + 1) % RLWorkshop.size();
        } else {
            currentWorkshopIndex = (currentWorkshopIndex - 1 + RLWorkshop.size()) % RLWorkshop.size();
        }
    }
    // Caller (SuiteSpot hotkey handler) will update the appropriate CVar
}

std::string MapManager::GetCurrentFreeplayCode() const
{
    if (!RLMaps.empty() && currentFreeplayIndex >= 0 && currentFreeplayIndex < (int)RLMaps.size())
        return RLMaps[currentFreeplayIndex].code;
    return "";
}

std::string MapManager::GetCurrentTrainingCode() const
{
    if (!RLTraining.empty() && currentTrainingIndex >= 0 && currentTrainingIndex < (int)RLTraining.size())
        return RLTraining[currentTrainingIndex].code;
    return "";
}

std::string MapManager::GetCurrentWorkshopPath() const
{
    if (!RLWorkshop.empty() && currentWorkshopIndex >= 0 && currentWorkshopIndex < (int)RLWorkshop.size())
        return RLWorkshop[currentWorkshopIndex].filePath;
    return "";
}