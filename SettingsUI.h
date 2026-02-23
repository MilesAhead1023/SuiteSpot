#pragma once
#include "IMGUI/imgui.h"
#include "StatusMessageUI.h"
#include "WorkshopDownloader.h"
#include <string>
#include <vector>
#include <map>

/*
 * ======================================================================================
 * SETTINGS UI: THE CONFIGURATION MENU
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class builds the visual menu you see when you press F2 in Rocket League and
 * click on the "SuiteSpot" tab.
 * 
 * WHY IS IT HERE?
 * Users need a way to change settings (like delay times or which map to load).
 * This class translates those settings into buttons, sliders, and checkboxes.
 * 
 * HOW DOES IT WORK?
 * 1. `RenderMainSettingsWindow()`: This is the main loop. It draws the tabs ("Main Settings",
 *    "Auto-Queue", etc.).
 * 2. It reads the current values from `SuiteSpot` (which gets them from `SettingsSync`).
 * 3. When you change a value (like sliding a slider), it immediately tells `SettingsSync`
 *    to save that new value to the config file.
 * 4. It uses `UI::Helpers` to make the code cleaner (so we don't have to write 
 *    "Draw Slider" logic 50 times).
 */

class SuiteSpot;

class SettingsUI
{
  public:
    explicit SettingsUI(SuiteSpot* plugin);
    void RenderMainSettingsWindow();

  private:
    SuiteSpot* plugin_;
    UI::StatusMessage statusMessage; // Shows "Success!" or error messages

    // Internal helpers to draw specific tabs
    // These break the big menu into smaller, manageable chunks
    void RenderGeneralTab(bool& enabledValue, int& mapTypeValue);
    void RenderMapSelectionTab(int mapTypeValue, std::string& currentFreeplayCode, std::string& currentTrainingCode,
                               std::string& currentWorkshopPath, int& delayFreeplaySecValue, int& delayTrainingSecValue,
                               int& delayWorkshopSecValue, int& delayQueueSecValue);
    void RenderFreeplayMode(std::string& currentFreeplayCode);
    void RenderTrainingMode(int trainingModeValue, std::string& currentTrainingCode);
    void RenderWorkshopMode(std::string& currentWorkshopPath);
    void RenderInstalledMaps(std::string& currentWorkshopPath);

    void RenderSinglePackMode(std::string& currentTrainingCode);
    std::vector<std::string> GetQuickPicksList();

    // Workshop browser tab
    void RenderWorkshopBrowserTab();
    void RebuildDisplayList(); // Re-scores and sorts displayResultList from cachedResultList
    void RenderTextureCheck();
    void RenderDownloadTexturesPopup(const std::vector<std::string>& missingFiles);

    void RLMAPS_RenderSearchWorkshopResults(const char* mapspath);
    void RenderReleases(RLMAPS_MapResult mapResult, const char* mapspath);
    void RenderAcceptDownload();
    void RenderYesNoPopup(const char* popupName, const char* label, std::function<void()> yesFunc,
                          std::function<void()> noFunc);
    void RenderInfoPopup(const char* popupName, const char* label);
    void CenterNextItem(float itemWidth);
    std::string LimitTextSize(std::string str, float maxTextSize);

    // Workshop path configuration state
    char workshopPathBuf[512] = {0};
    bool workshopPathInit = false;
    std::string workshopPathCache = "";

    // Workshop browser state
    char workshopSearchBuf[256] = {0};
    char workshopDownloadPathBuf[512] = {0};

    // Workshop local browser state (two-panel layout)
    int selectedWorkshopIndex = -1;       // Currently selected in list
    std::string lastSelectedWorkshopPath; // Track path to detect changes

    // Workshop browser (RLMAPS) state
    int selectedBrowserIndex = -1;                   // Indexes into displayResultList
    std::vector<RLMAPS_MapResult> cachedResultList;  // Unmodified API results (source of truth)
    std::vector<RLMAPS_MapResult> displayResultList; // Ranked/filtered view — what the UI renders
    int lastListVersion = -1;                        // Track version to know when to refresh cache
    char localFilterBuf[256] = {0};                  // Real-time local filter (re-ranks without API call)
    std::string lastLocalFilter;                     // Detect when filter text changes

    // Image cache - persists across list refreshes, keyed by map ID
    std::map<std::string, std::shared_ptr<ImageWrapper>> workshopImageCache;

    // Texture popup state
    bool showTexturePopup = false;
};
