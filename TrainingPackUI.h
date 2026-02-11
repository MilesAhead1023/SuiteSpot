#pragma once
#include "IMGUI/imgui.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "MapList.h"
#include "StatusMessageUI.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstring>

// Payload structure for dragging packs FROM bags (includes source bag info)
// This allows the drop target to know where the pack came from for removal
struct BagPackPayload {
    char packCode[32];
    char sourceBag[32];
};

/*
 * ======================================================================================
 * TRAINING PACK UI: THE BROWSER WINDOW
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class defines the "Training Pack Browser" window. Unlike the Settings UI, this
 * window can float anywhere on your screen and stay open while you play.
 * 
 * WHY IS IT HERE?
 * Players need a powerful tool to search, sort, and organize hundreds of training packs.
 * A simple dropdown menu isn't enough. This provides a full table view.
 * 
 * HOW DOES IT WORK?
 * 1. INHERITANCE: It inherits from `PluginWindow`, which tells BakkesMod "I am a separate window."
 * 2. `Render()`: This function draws the window content (Search bar, Table of packs, Buttons).
 *    - It uses `ImGui::Columns` to create the table layout.
 *    - It uses `ImGuiListClipper` to efficiently draw 2000+ packs without lagging the game.
 * 3. STATE: It remembers your search text, sort order, and which packs you've selected.
 */

class SuiteSpot;

class TrainingPackUI : public BakkesMod::Plugin::PluginWindow {
public:
    explicit TrainingPackUI(SuiteSpot* plugin);
    
    // PluginWindow implementation
    // The "Render" function is the main loop that draws the window every frame
    void Render() override;
    
    // Internal name used by BakkesMod to identify this window (e.g., for console commands)
    std::string GetMenuName() override;
    
    // The title text displayed at the top of the window
    std::string GetMenuTitle() override;
    
    // Boilerplate to ensure ImGui knows "where" to draw
    void SetImGuiContext(uintptr_t ctx) override;
    
    // Control whether the window steals mouse input
    bool ShouldBlockInput() override;
    
    // Check if the window is currently focused
    bool IsActiveOverlay() override;
    
    // Called when the window opens or closes
    void OnOpen() override;
    void OnClose() override;

    // Visibility control - Used by the main plugin to toggle this window
    bool IsOpen();
    void SetOpen(bool open);

private:
    // Helper: Check if code column should be shown (search contains digits)
    bool ShouldShowCodeColumn() const;
    SuiteSpot* plugin_;
    bool isWindowOpen_ = false;
    bool needsFocusOnNextRender_ = false;  // Bring to front when first opened

    char packSearchText[256] = {0};
    std::string packDifficultyFilter = "All";
    std::string packTagFilter = "";
    int packMinShots = 0;
    int packMaxShots = 100;
    int packSortColumn = 0;
    bool packSortAscending = true;
    bool packVideoFilter = false;  // Filter for packs with video URLs

    char lastSearchText[256] = {0};
    std::string lastDifficultyFilter = "All";
    std::string lastTagFilter = "";
    int lastMinShots = 0;
    int lastSortColumn = 0;
    bool lastSortAscending = true;
    bool lastVideoFilter = false;

    std::vector<std::string> availableTags;
    bool tagsInitialized = false;
    int lastPackCount = 0;
    std::vector<TrainingEntry> filteredPacks;

    // Selection state
    std::string selectedPackCode;
    int lastSelectedRowIndex = -1;
    bool packListInitialized = false;

    // Custom pack form state
    char customPackCode[20] = {0};       // XXXX-XXXX-XXXX-XXXX format
    char customPackName[128] = {0};
    char customPackCreator[64] = {0};
    int customPackDifficulty = 0;        // Index into difficulties array
    int customPackShotCount = 10;
    char customPackTags[256] = {0};      // Comma-separated
    char customPackNotes[512] = {0};
    char customPackVideoUrl[256] = {0};
    UI::StatusMessage customPackStatus;
    UI::StatusMessage browserStatus;

    // Helper methods
    void LoadPackImmediately(const std::string& packCode);
    void RenderCustomPackForm();
    bool ValidatePackCode(const char* code) const;
    void ClearCustomPackForm();
    void CalculateOptimalColumnWidths();

    // Column sizing state
    std::vector<float> columnWidths;
    bool columnWidthsDirty = true;
    bool columnWidthsInitialized = false;
    float lastWindowWidth = 0.0f;

    // Icons
    std::shared_ptr<ImageWrapper> youtubeIcon;

    // State sync
    std::string lastQuickPicksSelected;
};