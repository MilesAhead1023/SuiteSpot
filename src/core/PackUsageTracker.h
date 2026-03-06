#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <mutex>
#include <cstdint>

struct PackUsageStats
{
    std::string code;
    int loadCount = 0;
    int64_t lastLoadedTimestamp = 0;
};

class PackUsageTracker
{
  public:
    explicit PackUsageTracker(const std::filesystem::path& statsFilePath);

    void LoadStats();
    void SaveStats() const;
    void IncrementLoadCount(const std::string& packCode);
    int GetLoadCount(const std::string& packCode) const;
    int64_t GetLastPlayedTimestamp(const std::string& packCode) const;
    std::vector<std::string> GetTopUsedCodes(int count) const;
    bool IsFirstRun() const { return isFirstRun; }

  private:
    std::filesystem::path filePath;
    std::map<std::string, PackUsageStats> stats;
    bool isFirstRun = true;
    mutable std::mutex mutex_;
};
