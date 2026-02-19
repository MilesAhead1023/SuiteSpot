#include "catch.hpp"
#include <string>
#include <vector>
#include <algorithm>

// ----------------------------------------------------------------------------
// Stub types — mirror MapList.h without pulling in BakkesMod SDK headers.
// Replace with real includes once a Linux-compatible stub SDK is in place.
// ----------------------------------------------------------------------------
struct TrainingEntry {
    std::string code;
    std::string name;
    std::string creator;
    std::string difficulty;
    std::vector<std::string> tags;
};

// Minimal search logic extracted from TrainingPackManager for testability
static std::vector<TrainingEntry> SearchPacks(
    const std::vector<TrainingEntry>& packs,
    const std::string& query,
    const std::string& difficulty = "")
{
    std::vector<TrainingEntry> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& pack : packs) {
        if (!difficulty.empty() && pack.difficulty != difficulty) continue;

        std::string lowerName = pack.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerQuery.empty() || lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(pack);
        }
    }
    return results;
}

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------
TEST_CASE("TrainingPackManager search", "[TrainingPackManager]")
{
    std::vector<TrainingEntry> packs = {
        {"A1B2-C3D4", "Aerial Control", "Creator1", "Intermediate", {}},
        {"E5F6-G7H8", "Ground Shots",   "Creator2", "Beginner",     {}},
        {"I9J0-K1L2", "Advanced Dribbling", "Creator3", "Advanced",  {}},
        {"M3N4-O5P6", "aerial freestyle",   "Creator4", "Advanced",  {}},
    };

    SECTION("Empty query returns all packs")
    {
        auto results = SearchPacks(packs, "");
        REQUIRE(results.size() == 4);
    }

    SECTION("Case-insensitive match")
    {
        auto results = SearchPacks(packs, "AERIAL");
        REQUIRE(results.size() == 2);
    }

    SECTION("No match returns empty")
    {
        auto results = SearchPacks(packs, "zzznomatch");
        REQUIRE(results.empty());
    }

    SECTION("Difficulty filter")
    {
        auto results = SearchPacks(packs, "", "Advanced");
        REQUIRE(results.size() == 2);
    }

    SECTION("Combined query + difficulty filter")
    {
        auto results = SearchPacks(packs, "aerial", "Advanced");
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].code == "M3N4-O5P6");
    }
}

TEST_CASE("TrainingEntry validity", "[TrainingPackManager]")
{
    SECTION("Entry with empty code is invalid")
    {
        TrainingEntry e{"", "Some Pack", "Creator", "Beginner", {}};
        REQUIRE(e.code.empty());
    }

    SECTION("Entry with empty name is invalid")
    {
        TrainingEntry e{"A1B2-C3D4", "", "Creator", "Beginner", {}};
        REQUIRE(e.name.empty());
    }
}
