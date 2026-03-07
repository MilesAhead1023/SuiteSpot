#pragma once
#include "imgui.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "MapList.h"
#include "StatusMessageUI.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <cstring>

// Payload structure for dragging packs FROM bags (includes source bag info)
// This allows the drop target to know where the pack came from for removal
struct BagPackPayload
{
    char packCode[32];
    char sourceBag[32];
};

/*
 * ======================================================================================
 * TRAINING PACK UI: THE BROWSER WINDOW
 * ======================================================================================
 *
 * Two-panel floating browser window.
 *   Left  (60%): filterable/sortable selectable list with ImGuiListClipper
 *   Right (40%): selected-pack detail panel — thumbnail, metadata, action buttons
 *
 * Custom-pack creation lives in a modal popup opened from the left panel footer.
 */

class SuiteSpot;

class TrainingPackUI : public BakkesMod::Plugin::PluginWindow
{
  public:
    explicit TrainingPackUI(SuiteSpot* plugin);

    void Render() override;
    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    void SetImGuiContext(uintptr_t ctx) override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    void OnOpen() override;
    void OnClose() override;

    bool IsOpen();
    void SetOpen(bool open);

  private:
    SuiteSpot* plugin_;
    bool isWindowOpen_ = false;
    bool needsFocusOnNextRender_ = false;

    // ── Filter state ──────────────────────────────────────────────────
    char packSearchText[256] = {0};
    std::string packDifficultyFilter = "All";
    std::string packTagFilter;
    int packMinShots = 0;
    int packMaxShots = 50;
    bool packVideoFilter = false;
    int packSortColumn = 0;
    bool packSortAscending = true;

    // Cached last-seen values to detect changes without strcmp every frame
    char lastSearchText[256] = {0};
    std::string lastDifficultyFilter = "All";
    std::string lastTagFilter;
    int lastMinShots = 0;
    int lastMaxShots = 50;
    bool lastVideoFilter = false;
    int lastSortColumn = 0;
    bool lastSortAscending = true;

    // ── Filtered list cache ───────────────────────────────────────────
    std::vector<TrainingEntry> filteredPacks;
    bool packListInitialized = false;
    int lastPackCount = 0;

    // Tag dropdown population
    std::vector<std::string> availableTags;
    bool tagsInitialized = false;

    // ── Selection ─────────────────────────────────────────────────────
    std::string selectedPackCode;
    std::string lastQuickPicksSelected;

    // ── Thumbnail fetch ───────────────────────────────────────────────
    std::filesystem::path thumbnailCacheDir_;
    std::string lastFetchedThumbnailCode_;      // code we last triggered a fetch for
    std::shared_ptr<ImageWrapper> youtubeIcon_; // small YouTube logo for list indicator

    // Returns YouTube video ID from various URL formats, or "" if not a YouTube URL
    static std::string ExtractYouTubeId(const std::string& url);
    static std::string ExtractImgurId(const std::string& url);

    // Triggers async thumbnail download for the selected pack (idempotent)
    void FetchThumbnailForSelected();

    // ── Custom pack modal ────────────────────────────────────────────
    bool showAddPackModal_ = false;

    char customPackCode[20] = {0};
    char customPackName[128] = {0};
    char customPackCreator[64] = {0};
    int customPackDifficulty = 0;
    int customPackShotCount = 10;
    char customPackTags[256] = {0};
    char customPackNotes[512] = {0};
    char customPackVideoUrl[256] = {0};
    UI::StatusMessage customPackStatus;

    // ── Status ───────────────────────────────────────────────────────
    UI::StatusMessage browserStatus;

    // ── Helpers ──────────────────────────────────────────────────────
    void LoadPackImmediately(const std::string& packCode);
    void RenderDetailPanel(const TrainingEntry* pack);
    void RenderCustomPackModal();
    bool ValidatePackCode(const char* code) const;
    void ClearCustomPackForm();

    // Returns difficulty color for text/badges (same mapping as before)
    static ImVec4 DifficultyColor(const std::string& difficulty);
};