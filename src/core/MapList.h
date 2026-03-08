#pragma once
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <memory>
#include "bakkesmod/plugin/bakkesmodplugin.h"

// Freeplay maps
struct MapEntry
{
    std::string code;
    std::string name;
};
extern std::vector<MapEntry> SuiteMaps;

// Training packs
struct TrainingEntry
{
    std::string code;
    std::string name;

    // Pack metadata
    std::string creator;           // Creator's display name
    std::string creatorSlug;       // Creator's username (for linking)
    std::string difficulty;        // Bronze, Gold, Platinum, Diamond, Champion, Supersonic Legend
    std::vector<std::string> tags; // Array of tags
    int shotCount = 0;             // Number of shots
    std::string staffComments;     // Staff description
    std::string notes;             // Creator's notes
    std::string videoUrl;          // Optional YouTube tutorial link
    std::string gifUrl;            // Optional imgur preview clip (mp4)
    int likes = 0;                 // Engagement metric
    int plays = 0;                 // Engagement metric
    int status = 1;                // Pack status (1 = active)

    // Unified system fields
    std::string source = "prejump"; // "prejump" or "custom"
    bool isModified = false;        // Track if user edited a scraped pack

    // Thumbnail cache (loaded lazily when pack is selected in browser)
    std::shared_ptr<ImageWrapper> thumbnailImage;
    bool isThumbnailRequested = false; // Prevents duplicate HTTP fetches
};
extern std::vector<TrainingEntry> SuiteTraining;

// Workshop maps
struct WorkshopEntry
{
    std::string filePath;                       // UPK file path
    std::string name;                           // Display name
    std::string author;                         // Map author (from JSON)
    std::string description;                    // Map description (from JSON)
    std::filesystem::path folder;               // Map folder path
    std::filesystem::path previewPath;          // Preview image path (.jfif, .jpg, .png)
    std::shared_ptr<ImageWrapper> previewImage; // Loaded image
    bool isImageLoaded = false;
};
extern std::vector<WorkshopEntry> SuiteWorkshop;