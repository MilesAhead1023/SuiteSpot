#include "bakkesmod/wrappers/gfx/GfxDataTrainingWrapper.h"
#include "bakkesmod/wrappers/GameEvent/SaveData/TrainingEditorSaveDataWrapper.h"
#include "pch.h"
#include "TrainingPackManager.h"
#include "EmbeddedPackGrabber.h"
#include "ProcessUtils.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <thread>

void TrainingPackManager::LoadPacksFromFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath)) {
        LOG("SuiteSpot: Pack cache file not found: {}", filePath.string());
        {
            std::lock_guard<std::mutex> lock(packMutex);
            RLTraining.clear();
            packCount = 0;
        }
        lastUpdated = "Never";
        return;
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG("SuiteSpot: Failed to open Pack cache file");
            return;
        }

        nlohmann::json jsonData;
        file >> jsonData;
        file.close();

        std::lock_guard<std::mutex> lock(packMutex);
        RLTraining.clear();

        if (!jsonData.contains("packs") || !jsonData["packs"].is_array()) {
            LOG("SuiteSpot: Invalid Pack cache file format - missing 'packs' array");
            return;
        }

        for (const auto& pack : jsonData["packs"]) {
            TrainingEntry entry;

            if (pack.contains("code") && pack["code"].is_string()) {
                entry.code = pack["code"].get<std::string>();
            }
            if (pack.contains("name") && pack["name"].is_string()) {
                entry.name = pack["name"].get<std::string>();
            }

            if (entry.code.empty() || entry.name.empty()) {
                continue;
            }

            if (pack.contains("creator") && pack["creator"].is_string()) {
                entry.creator = pack["creator"].get<std::string>();
            }
            if (pack.contains("creatorSlug") && pack["creatorSlug"].is_string()) {
                entry.creatorSlug = pack["creatorSlug"].get<std::string>();
            }
            if (pack.contains("difficulty") && pack["difficulty"].is_string()) {
                entry.difficulty = pack["difficulty"].get<std::string>();
            }
            if (pack.contains("shotCount") && pack["shotCount"].is_number()) {
                entry.shotCount = pack["shotCount"].get<int>();
            }
            if (pack.contains("staffComments") && pack["staffComments"].is_string()) {
                entry.staffComments = pack["staffComments"].get<std::string>();
            }
            if (pack.contains("notes") && pack["notes"].is_string()) {
                entry.notes = pack["notes"].get<std::string>();
            }
            if (pack.contains("videoUrl") && pack["videoUrl"].is_string()) {
                entry.videoUrl = pack["videoUrl"].get<std::string>();
            }
            if (pack.contains("likes") && pack["likes"].is_number()) {
                entry.likes = pack["likes"].get<int>();
            }
            if (pack.contains("plays") && pack["plays"].is_number()) {
                entry.plays = pack["plays"].get<int>();
            }
            if (pack.contains("status") && pack["status"].is_number()) {
                entry.status = pack["status"].get<int>();
            }

            if (pack.contains("tags") && pack["tags"].is_array()) {
                for (const auto& tag : pack["tags"]) {
                    if (tag.is_string()) {
                        entry.tags.push_back(tag.get<std::string>());
                    }
                }
            }

            // Unified system fields
            if (pack.contains("source") && pack["source"].is_string()) {
                entry.source = pack["source"].get<std::string>();
            } else {
                entry.source = "prejump"; // Default for backward compatibility
            }
            
            // Bag categories removed - skip loading bagCategories and orderInBag
            
            if (pack.contains("isModified") && pack["isModified"].is_boolean()) {
                entry.isModified = pack["isModified"].get<bool>();
            }

            RLTraining.push_back(entry);
        }

        // Sort RLTraining alphabetically by name
        std::sort(RLTraining.begin(), RLTraining.end(), [](const TrainingEntry& a, const TrainingEntry& b) {
            // Case-insensitive comparison for names
            std::string nameA = a.name;
            std::string nameB = b.name;
            std::transform(nameA.begin(), nameA.end(), nameA.begin(), [](unsigned char c) { return std::tolower(c); });
            std::transform(nameB.begin(), nameB.end(), nameB.begin(), [](unsigned char c) { return std::tolower(c); });
            return nameA < nameB;
        });

        packCount = static_cast<int>(RLTraining.size());
        lastUpdated = GetLastUpdatedTime(filePath);
        currentFilePath = filePath;

        LOG("SuiteSpot: Loaded {} training packs from file", packCount);

    } catch (const std::exception& e) {
        LOG("SuiteSpot: Error loading training packs: {}", std::string(e.what()));
        {
            std::lock_guard<std::mutex> lock(packMutex);
            RLTraining.clear();
            packCount = 0;
        }
    }
}

bool TrainingPackManager::IsCacheStale(const std::filesystem::path& filePath) const
{
    if (!std::filesystem::exists(filePath)) {
        return true;
    }

    try {
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        auto now = std::filesystem::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWriteTime);

        return age.count() > 168;
    } catch (...) {
        return true;
    }
}

std::string TrainingPackManager::GetLastUpdatedTime(const std::filesystem::path& filePath) const
{
    if (!std::filesystem::exists(filePath)) {
        return "Never";
    }

    try {
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lastWriteTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto tt = std::chrono::system_clock::to_time_t(sctp);

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M UTC");
        return oss.str();
    } catch (...) {
        return "Unknown";
    }
}

void TrainingPackManager::UpdateTrainingPackList(const std::filesystem::path& outputPath,
                                                   const std::shared_ptr<GameWrapper>& gameWrapper)
{
    if (scrapingInProgress) {
        LOG("SuiteSpot: Training pack update already in progress");
        return;
    }

    if (!gameWrapper) {
        LOG("SuiteSpot: GameWrapper unavailable for training pack update");
        return;
    }

    scrapingInProgress = true;

    LOG("SuiteSpot: Training pack updater starting");
    LOG("SuiteSpot: Output path: {}", outputPath.string());

    // Join previous update thread if still running
    if (updateThread.joinable()) updateThread.join();

    // Launch update in background thread to avoid blocking game thread
    updateThread = std::thread([this, outputPath]() {
        try {
            // Get temp directory for script extraction
            std::filesystem::path tempDir = std::filesystem::temp_directory_path();
            auto tempScript = tempDir / "SuitePackGrabber_temp.ps1";
            auto logFile = outputPath.parent_path() / "PackGrabber.log";

            // Write embedded script to temp file
            {
                std::ofstream tempFile(tempScript, std::ios::binary);
                if (!tempFile.is_open()) {
                    LOG("SuiteSpot: Failed to create temp script file: {}", tempScript.string());
                    scrapingInProgress = false;
                    return;
                }
                tempFile.write(EMBEDDED_PACK_GRABBER_SCRIPT,
                              std::string_view(EMBEDDED_PACK_GRABBER_SCRIPT).length());
                tempFile.close();
            }

            std::string outStr = outputPath.string();

            LOG("SuiteSpot: Training pack updater started");

            std::string scriptArgs = "-OutputPath \"" + outStr + "\"";
            int result = Utils::RunPowerShellScript(tempScript.string(), scriptArgs, logFile.string());

            LOG("SuiteSpot: Training pack updater returned: {}", result);

            // Try to read error log if it exists
            if (std::filesystem::exists(logFile)) {
                std::ifstream log(logFile);
                std::string line;
                LOG("SuiteSpot: PackGrabber output:");
                while (std::getline(log, line)) {
                    LOG("  {}", line);
                }
            }

            if (result == 0) {
                LOG("SuiteSpot: Training pack update completed successfully");
                LoadPacksFromFile(outputPath);
                lastUpdated = GetLastUpdatedTime(outputPath);
            } else {
                LOG("SuiteSpot: Training pack updater returned non-zero exit code: {}", result);
            }

            // Clean up temp script
            try {
                if (std::filesystem::exists(tempScript)) {
                    std::filesystem::remove(tempScript);
                    LOG("SuiteSpot: Cleaned up temporary script file");
                }
            } catch (const std::exception& e) {
                LOG("SuiteSpot: Warning - failed to clean up temp script: {}", std::string(e.what()));
            }

        } catch (const std::exception& e) {
            LOG("SuiteSpot: Training pack updater error: {}", std::string(e.what()));
        }

        scrapingInProgress = false;
    });
}

void TrainingPackManager::FilterAndSortPacks(const std::string& searchText,
                                            const std::string& difficultyFilter,
                                            const std::string& tagFilter,
                                            int minShots,
                                            bool videoFilter,
                                            int sortColumn,
                                            bool sortAscending,
                                            std::vector<TrainingEntry>& out) const
{
    std::lock_guard<std::mutex> lock(packMutex);
    out.clear();

    std::string searchLower(searchText);
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& pack : RLTraining) {
        // Video filter - skip packs without video if filter is enabled
        if (videoFilter && pack.videoUrl.empty()) {
            continue;
        }

        if (!searchLower.empty()) {
            // Search only by NAME or CODE
            bool matchesSearch = false;

            // 1. Search by name (case-insensitive, partial match)
            std::string nameLower = pack.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (nameLower.find(searchLower) != std::string::npos) {
                matchesSearch = true;
            }

            // 2. Search by code (supports both formatted "XXXX-XXXX-XXXX-XXXX" and unformatted 16-char)
            if (!matchesSearch) {
                // Remove all dashes from both search term and pack code for comparison
                std::string codeLower = pack.code;
                std::string searchNoDashes = searchLower;
                std::transform(codeLower.begin(), codeLower.end(), codeLower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                
                // Strip dashes from both strings
                codeLower.erase(std::remove(codeLower.begin(), codeLower.end(), '-'), codeLower.end());
                searchNoDashes.erase(std::remove(searchNoDashes.begin(), searchNoDashes.end(), '-'), searchNoDashes.end());
                
                // Match if search term is contained in code (allows partial code search)
                if (codeLower.find(searchNoDashes) != std::string::npos) {
                    matchesSearch = true;
                }
            }

            if (!matchesSearch) {
                continue;
            }
        }

        if (difficultyFilter != "All") {
            if (difficultyFilter == "Unranked") {
                // Match "Unranked" filter against all "no difficulty" values
                if (!pack.difficulty.empty() && pack.difficulty != "Unknown" && pack.difficulty != "All" && pack.difficulty != "Unranked") {
                    continue;
                }
            } else {
                // Standard strict match for ranked tiers
                if (pack.difficulty != difficultyFilter) {
                    continue;
                }
            }
        }

        if (!tagFilter.empty()) {
            bool hasTag = false;
            for (const auto& tag : pack.tags) {
                if (tag == tagFilter) {
                    hasTag = true;
                    break;
                }
            }
            if (!hasTag) continue;
        }

        if (pack.shotCount < minShots) {
            continue;
        }

        out.push_back(pack);
    }

    // Case-insensitive string comparison helper
    auto caseInsensitiveCompare = [](const std::string& a, const std::string& b) -> int {
        std::string lowerA = a;
        std::string lowerB = b;
        std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), [](unsigned char c) { return std::tolower(c); });
        return lowerA.compare(lowerB);
    };

    // Difficulty ranking helper (lower rank = easier)
    auto getDifficultyRank = [](const std::string& difficulty) -> int {
        std::string diffLower = difficulty;
        std::transform(diffLower.begin(), diffLower.end(), diffLower.begin(), [](unsigned char c) { return std::tolower(c); });
        
        if (diffLower.empty() || diffLower == "unknown" || diffLower == "all" || diffLower == "unranked") return 0;
        if (diffLower == "bronze") return 1;
        if (diffLower == "silver") return 2;
        if (diffLower == "gold") return 3;
        if (diffLower == "platinum") return 4;
        if (diffLower == "diamond") return 5;
        if (diffLower == "champion") return 6;
        if (diffLower == "grand champion") return 7;
        if (diffLower == "supersonic legend") return 8;
        return 0; // Unknown difficulties default to unranked
    };

    std::sort(out.begin(), out.end(), [sortColumn, sortAscending, &caseInsensitiveCompare, &getDifficultyRank](const TrainingEntry& a, const TrainingEntry& b) {
        int cmp = 0;
        switch (sortColumn) {
            case 0: // Name
                cmp = caseInsensitiveCompare(a.name, b.name);
                break;
            case 1: // Creator
                cmp = caseInsensitiveCompare(a.creator, b.creator);
                break;
            case 2: // Difficulty
                {
                    int rankA = getDifficultyRank(a.difficulty);
                    int rankB = getDifficultyRank(b.difficulty);
                    cmp = (rankA < rankB) ? -1 : (rankA > rankB) ? 1 : 0;
                }
                break;
            case 3: // Shots
                cmp = (a.shotCount < b.shotCount) ? -1 : (a.shotCount > b.shotCount) ? 1 : 0;
                break;
            case 4: // Likes
                cmp = (a.likes < b.likes) ? -1 : (a.likes > b.likes) ? 1 : 0;
                break;
            case 5: // Plays
                cmp = (a.plays < b.plays) ? -1 : (a.plays > b.plays) ? 1 : 0;
                break;
        }
        return sortAscending ? (cmp < 0) : (cmp > 0);
    });
}

void TrainingPackManager::BuildAvailableTags(std::vector<std::string>& out) const
{
    std::lock_guard<std::mutex> lock(packMutex);
    std::set<std::string> uniqueTags;
    for (const auto& pack : RLTraining) {
        for (const auto& tag : pack.tags) {
            uniqueTags.insert(tag);
        }
    }

    out.clear();
    out.push_back("All Tags");
    for (const auto& tag : uniqueTags) {
        out.push_back(tag);
    }
}

void TrainingPackManager::SavePacksToFile(const std::filesystem::path& filePath)
{
    try {
        std::lock_guard<std::mutex> lock(packMutex);
        nlohmann::json output;
        output["version"] = "1.0.0";

        // Get current time in ISO format
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
        output["lastUpdated"] = oss.str();

        output["source"] = "https://prejump.com/training-packs";
        output["totalPacks"] = RLTraining.size();

        nlohmann::json packsArray = nlohmann::json::array();
        for (const auto& pack : RLTraining) {
            nlohmann::json p;
            p["name"] = pack.name;
            p["code"] = pack.code;
            p["creator"] = pack.creator;
            p["creatorSlug"] = pack.creatorSlug;
            p["difficulty"] = pack.difficulty;
            p["shotCount"] = pack.shotCount;
            p["tags"] = pack.tags;
            p["videoUrl"] = pack.videoUrl;
            p["staffComments"] = pack.staffComments;
            p["notes"] = pack.notes;
            p["likes"] = pack.likes;
            p["plays"] = pack.plays;
            p["status"] = pack.status;

            // Unified system fields
            p["source"] = pack.source;
            // Bag categories removed - don't save bagCategories or orderInBag
            p["isModified"] = pack.isModified;

            packsArray.push_back(p);
        }
        output["packs"] = packsArray;

        // Ensure directory exists
        auto parentDir = filePath.parent_path();
        if (!std::filesystem::exists(parentDir)) {
            std::filesystem::create_directories(parentDir);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG("SuiteSpot: Failed to open file for writing: {}", filePath.string());
            return;
        }

        file << output.dump(2); // Pretty print with 2-space indent
        file.close();

        currentFilePath = filePath;
        lastUpdated = GetLastUpdatedTime(filePath);
        LOG("SuiteSpot: Saved {} packs to file", RLTraining.size());

    } catch (const std::exception& e) {
        LOG("SuiteSpot: Error saving packs: {}", std::string(e.what()));
    }
}

bool TrainingPackManager::AddCustomPack(const TrainingEntry& pack)
{
    {
        std::lock_guard<std::mutex> lock(packMutex);
        // Check for duplicate code
        for (const auto& existing : RLTraining) {
            if (existing.code == pack.code) {
                LOG("SuiteSpot: Pack with code {} already exists", pack.code);
                return false;
            }
        }

        TrainingEntry newPack = pack;
        newPack.source = "custom";
        RLTraining.push_back(newPack);

        // Sort RLTraining alphabetically by name
        std::sort(RLTraining.begin(), RLTraining.end(), [](const TrainingEntry& a, const TrainingEntry& b) {
            std::string nameA = a.name;
            std::string nameB = b.name;
            std::transform(nameA.begin(), nameA.end(), nameA.begin(), [](unsigned char c) { return std::tolower(c); });
            std::transform(nameB.begin(), nameB.end(), nameB.begin(), [](unsigned char c) { return std::tolower(c); });
            return nameA < nameB;
        });

        packCount = static_cast<int>(RLTraining.size());
        LOG("SuiteSpot: Added custom pack: {}", pack.name);
        // Lock releases here before SavePacksToFile
    }

    // Auto-save if we have a file path (outside the lock)
    if (!currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
    }
    return true;
}

bool TrainingPackManager::UpdatePack(const std::string& code, const TrainingEntry& updatedPack)
{
    {
        std::lock_guard<std::mutex> lock(packMutex);
        for (auto& pack : RLTraining) {
            if (pack.code == code) {
                // Preserve source and update isModified
                std::string originalSource = pack.source;
                pack = updatedPack;
                pack.source = originalSource;

                // Mark as modified if it was a prejump pack
                if (originalSource == "prejump") {
                    pack.isModified = true;
                }

                // Sort RLTraining alphabetically by name
                std::sort(RLTraining.begin(), RLTraining.end(), [](const TrainingEntry& a, const TrainingEntry& b) {
                    std::string nameA = a.name;
                    std::string nameB = b.name;
                    std::transform(nameA.begin(), nameA.end(), nameA.begin(), [](unsigned char c) { return std::tolower(c); });
                    std::transform(nameB.begin(), nameB.end(), nameB.begin(), [](unsigned char c) { return std::tolower(c); });
                    return nameA < nameB;
                });

                LOG("SuiteSpot: Updated pack: {}", pack.name);
                // Lock releases here before SavePacksToFile
            }
        }
    }

    // Auto-save outside the lock
    if (!currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
        return true;
    }
    return false;
}

bool TrainingPackManager::DeletePack(const std::string& code)
{
    std::string name;
    {
        std::lock_guard<std::mutex> lock(packMutex);
        auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
            [&code](const TrainingEntry& p) { return p.code == code; });

        if (it != RLTraining.end()) {
            name = it->name;
            RLTraining.erase(it);
            packCount = static_cast<int>(RLTraining.size());

            LOG("SuiteSpot: Deleted pack: {}", name);
        } else {
            return false;
        }
    }

    // Auto-save outside the lock
    if (!currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
    }
    return true;
}

// ============================================================================
// CATEGORIZED BAG SYSTEM
// ============================================================================

void TrainingPackManager::HealPack(const std::string& code, int shots)
{
    if (code.empty() || shots <= 0) {
        LOG("SuiteSpot: HealPack called with invalid data - code: {}, shots: {}", code, shots);
        return;
    }

    bool needsSave = false;
    bool packFound = false;
    {
        std::lock_guard<std::mutex> lock(packMutex);
        for (auto& pack : RLTraining) {
            if (pack.code == code) {
                packFound = true;
                // Heal if shot count is missing or incorrect
                if (pack.shotCount <= 0 || pack.shotCount != shots) {
                    int oldCount = pack.shotCount;
                    pack.shotCount = shots;
                    needsSave = true;
                    LOG("SuiteSpot: Healed pack '{}' ({}): {} -> {} shots", pack.name, code, oldCount, shots);
                } else {
                    LOG("SuiteSpot: Pack '{}' ({}) already has correct shot count: {}", pack.name, code, shots);
                }
                break;
            }
        }
    }

    if (!packFound) {
        LOG("SuiteSpot: HealPack - Pack not found in database: {}", code);
    }

    if (needsSave && !currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
        LOG("SuiteSpot: Saved healed pack data to: {}", currentFilePath.string());
    }
}

std::optional<TrainingEntry> TrainingPackManager::GetPackByCode(const std::string& code) const
{
    std::lock_guard<std::mutex> lock(packMutex);
    for (const auto& pack : RLTraining) {
        if (pack.code == code) {
            return pack;
        }
    }
    return std::nullopt;
}
