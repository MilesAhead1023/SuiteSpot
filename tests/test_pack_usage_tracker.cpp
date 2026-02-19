#include "catch.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono>

// ----------------------------------------------------------------------------
// Minimal stub of PackUsageTracker logic for unit testing
// ----------------------------------------------------------------------------
struct UsageRecord {
    std::string code;
    int useCount   = 0;
    bool favorited = false;
    long long lastUsedMs = 0;
};

class UsageTrackerStub
{
public:
    void RecordUse(const std::string& code)
    {
        auto& rec = records_[code];
        rec.code = code;
        rec.useCount++;
        rec.lastUsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void SetFavorite(const std::string& code, bool fav)
    {
        records_[code].favorited = fav;
    }

    int GetUseCount(const std::string& code) const
    {
        auto it = records_.find(code);
        return it != records_.end() ? it->second.useCount : 0;
    }

    bool IsFavorited(const std::string& code) const
    {
        auto it = records_.find(code);
        return it != records_.end() && it->second.favorited;
    }

    std::vector<std::string> GetTopN(int n) const
    {
        std::vector<std::pair<std::string, UsageRecord>> vec(records_.begin(), records_.end());
        std::sort(vec.begin(), vec.end(),
            [](const auto& a, const auto& b){ return a.second.useCount > b.second.useCount; });

        std::vector<std::string> result;
        for (int i = 0; i < n && i < (int)vec.size(); ++i)
            result.push_back(vec[i].first);
        return result;
    }

private:
    std::unordered_map<std::string, UsageRecord> records_;
};

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------
TEST_CASE("PackUsageTracker use counting", "[PackUsageTracker]")
{
    UsageTrackerStub tracker;

    SECTION("New code starts at 0")
    {
        REQUIRE(tracker.GetUseCount("A1B2-C3D4") == 0);
    }

    SECTION("RecordUse increments count")
    {
        tracker.RecordUse("A1B2-C3D4");
        tracker.RecordUse("A1B2-C3D4");
        REQUIRE(tracker.GetUseCount("A1B2-C3D4") == 2);
    }

    SECTION("Top N returns most used")
    {
        tracker.RecordUse("AAAA");
        tracker.RecordUse("BBBB");
        tracker.RecordUse("BBBB");
        tracker.RecordUse("BBBB");
        tracker.RecordUse("CCCC");
        tracker.RecordUse("CCCC");

        auto top = tracker.GetTopN(2);
        REQUIRE(top.size() == 2);
        REQUIRE(top[0] == "BBBB");
        REQUIRE(top[1] == "CCCC");
    }
}

TEST_CASE("PackUsageTracker favorites", "[PackUsageTracker]")
{
    UsageTrackerStub tracker;

    SECTION("Not favorited by default")
    {
        REQUIRE_FALSE(tracker.IsFavorited("X1Y2-Z3W4"));
    }

    SECTION("SetFavorite true")
    {
        tracker.SetFavorite("X1Y2-Z3W4", true);
        REQUIRE(tracker.IsFavorited("X1Y2-Z3W4"));
    }

    SECTION("SetFavorite false removes favorite")
    {
        tracker.SetFavorite("X1Y2-Z3W4", true);
        tracker.SetFavorite("X1Y2-Z3W4", false);
        REQUIRE_FALSE(tracker.IsFavorited("X1Y2-Z3W4"));
    }
}
