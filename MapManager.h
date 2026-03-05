#pragma once
#include "MapList.h"
#include "logging.h"
#include <filesystem>
#include <string>
#include <vector>

/*
 * ======================================================================================
 * MAP MANAGER: THE MAP FINDER
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class is responsible for finding maps on your computer.
 * 
 * WHY IS IT HERE?
 * Rocket League (and BakkesMod) don't automatically know where all your Workshop maps are,
 * especially if you downloaded them manually or use a custom folder. We need to scan
 * the disk to find them.
 * 
 * HOW DOES IT WORK?
 * 1. `DiscoverWorkshopInDir()`: You give it a folder (like "C:\MyMaps"), and it looks for
 *    files ending in `.upk` or `.udk`.
 * 2. It creates a list of these maps (`WorkshopEntry`) so the UI can display them.
 * 3. It helps other parts of the plugin figure out where the "Data" folder is.
 */

class MapManager
{
  public:
    MapManager();

    // Finds the main "Data" folder where we save our stuff
    std::filesystem::path GetDataRoot() const;

    // Finds the specific folder for SuiteSpot data
    std::filesystem::path GetSuiteSpotRoot() const;
    std::filesystem::path GetSuiteTrainingDir() const;

    // Workshop-specific paths
    std::filesystem::path GetWorkshopCacheDir() const;
    std::filesystem::path GetTexturesDir() const;

    // Training pack path
    std::filesystem::path GetTrainingPacksPath() const;

    // Makes sure these folders actually exist (creates them if missing)
    void EnsureDataDirectories() const;

    // Workshop Helpers
    std::filesystem::path GetWorkshopLoaderConfigPath() const;
    std::filesystem::path ResolveConfiguredWorkshopRoot() const;

    // The big scanner: Finds maps in a folder and adds them to the list
    void DiscoverWorkshopInDir(const std::filesystem::path& dir, std::vector<WorkshopEntry>& outList) const;

    // Refreshes the list of maps
    void LoadWorkshopMaps(std::vector<WorkshopEntry>& outList, int& currentIndex);

    // Hotkey cycling methods (used by hotkey system)
    // Note: These methods update internal indices and should be followed by
    // SettingsSync CVar updates to persist the selection
    void CycleMapMode(bool forward); // Cycle between Freeplay/Training/Workshop
    void CycleMap(bool forward);     // Cycle within current map type

    // Get current indices for cycling
    int GetCurrentMapModeIndex() const { return currentMapModeIndex; }
    int GetCurrentFreeplayIndex() const { return currentFreeplayIndex; }
    int GetCurrentTrainingIndex() const { return currentTrainingIndex; }
    int GetCurrentWorkshopIndex() const { return currentWorkshopIndex; }

    // Get the actual code/path for the current selection (use after CycleMap)
    std::string GetCurrentFreeplayCode() const;
    std::string GetCurrentTrainingCode() const;
    std::string GetCurrentWorkshopPath() const;

  private:
    std::filesystem::path dataRoot;

    // Cycling indices
    int currentMapModeIndex = 0; // 0=Freeplay, 1=Training, 2=Workshop
    int currentFreeplayIndex = 0;
    int currentTrainingIndex = 0;
    int currentWorkshopIndex = 0;

    // Parse workshop JSON metadata file
    bool LoadWorkshopMetadata(const std::filesystem::path& jsonPath, std::string& outTitle, std::string& outAuthor,
                              std::string& outDescription) const;

    // Find preview image in workshop folder (.jfif, .jpg, .png)
    std::filesystem::path FindPreviewImage(const std::filesystem::path& folder) const;
};