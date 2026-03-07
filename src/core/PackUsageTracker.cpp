#include "pch.h"
#include "PackUsageTracker.h"
#include "logging.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <chrono>

using json = nlohmann::json;

PackUsageTracker::PackUsageTracker(const std::filesystem::path& statsFilePath) : filePath(statsFilePath)
{
    LoadStats();
}

void PackUsageTracker::LoadStats()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!std::filesystem::exists(filePath)) {
        isFirstRun = true;
        return;
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return;

        json j;
        file >> j;

        if (j.contains("stats") && j["stats"].is_array()) {
            for (const auto& item : j["stats"]) {
                PackUsageStats s;
                s.code = item.value("code", "");
                s.loadCount = item.value("loadCount", 0);
                s.lastLoadedTimestamp = item.value("lastLoadedTimestamp", 0LL);

                if (!s.code.empty()) {
                    stats[s.code] = s;
                }
            }
            isFirstRun = stats.empty();
        }
    } catch (const std::exception& e) {
        LOG("Failed to load pack usage stats: {}", e.what());
    }
}

void PackUsageTracker::SaveStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        json j;
        j["version"] = "1.0.0";
        j["stats"] = json::array();

        for (const auto& [code, s] : stats) {
            j["stats"].push_back(
                {{"code", s.code}, {"loadCount", s.loadCount}, {"lastLoadedTimestamp", s.lastLoadedTimestamp}});
        }

        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << j.dump(4);
        }
    } catch (const std::exception& e) {
        LOG("Failed to save pack usage stats: {}", e.what());
    }
}

void PackUsageTracker::IncrementLoadCount(const std::string& packCode)
{
    if (packCode.empty()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& s = stats[packCode];
        s.code = packCode;
        s.loadCount++;
        s.lastLoadedTimestamp =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        isFirstRun = false;
    }

    SaveStats();
}

int PackUsageTracker::GetLoadCount(const std::string& packCode) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stats.find(packCode);
    return (it != stats.end()) ? it->second.loadCount : 0;
}

int64_t PackUsageTracker::GetLastPlayedTimestamp(const std::string& packCode) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stats.find(packCode);
    return (it != stats.end()) ? it->second.lastLoadedTimestamp : 0;
}

std::vector<std::string> PackUsageTracker::GetTopUsedCodes(int count) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PackUsageStats> allStats;
    for (const auto& [code, s] : stats) {
        allStats.push_back(s);
    }

    // Sort by load count (desc), then by last loaded timestamp (desc)
    std::sort(allStats.begin(), allStats.end(), [](const PackUsageStats& a, const PackUsageStats& b) {
        if (a.loadCount != b.loadCount) {
            return a.loadCount > b.loadCount;
        }
        return a.lastLoadedTimestamp > b.lastLoadedTimestamp;
    });

    std::vector<std::string> result;
    for (int i = 0; i < std::min(count, (int)allStats.size()); ++i) {
        result.push_back(allStats[i].code);
    }
    return result;
}
