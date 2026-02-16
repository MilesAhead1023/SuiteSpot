#include "pch.h"
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "TrainingPackUI.h"
#include "TrainingPackManager.h"
#include "SuiteSpot.h"
#include "SettingsSync.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

// Helper function for sortable column headers with visual indicators
namespace {
    bool SortableColumnHeader(const char* label, int columnIndex, int& currentSortColumn, bool& sortAscending) {
        // Display label with sort indicator if this column is active
        // Use ASCII arrows (^ v) since Unicode triangles may not be in the font
        char buffer[256];
        if (currentSortColumn == columnIndex) {
            snprintf(buffer, sizeof(buffer), "%s %s", label, sortAscending ? "(asc)" : "(desc)");
        } else {
            snprintf(buffer, sizeof(buffer), "%s", label);
        }

        bool clicked = ImGui::Selectable(buffer, currentSortColumn == columnIndex, ImGuiSelectableFlags_DontClosePopups);
        if (clicked) {
            if (currentSortColumn == columnIndex) {
                sortAscending = !sortAscending;
            } else {
                currentSortColumn = columnIndex;
                sortAscending = true;
            }
        }
        return clicked;
    }
}

TrainingPackUI::TrainingPackUI(SuiteSpot* plugin) : plugin_(plugin) {
    auto path = plugin_->GetDataRoot() / "SuiteSpot" / "Resources" / "Icons" / "icon_youtube.png";
    LOG("SuiteSpot: Attempting to load YouTube icon from: " + path.string());
    youtubeIcon = std::make_shared<ImageWrapper>(path.string(), true);
    youtubeIcon->LoadForImGui([this, path](bool success) {
        if (success) {
            LOG("SuiteSpot: YouTube icon loaded successfully.");
        } else {
            LOG("SuiteSpot: Failed to load YouTube icon from " + path.string());
        }
    });
}

void TrainingPackUI::Render() {
    if (!isWindowOpen_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    // Bring window to front when first opened
    if (needsFocusOnNextRender_) {
        ImGui::SetNextWindowFocus();
        needsFocusOnNextRender_ = false;
    }

    // Only prevent bringing to front when modals or special states need focus control
    ImGuiWindowFlags browserFlags = ImGuiWindowFlags_None;

    if (!ImGui::Begin(GetMenuTitle().c_str(), &isWindowOpen_, browserFlags)) {
        ImGui::End();
        return;
    }

    // Apply font scale to this window
    ImGui::SetWindowFontScale(UI::FONT_SCALE);

    const auto* manager = plugin_->trainingPackMgr.get();
    static const std::vector<TrainingEntry> emptyPacks;
    static const std::string emptyString;
    const auto& packs = manager ? manager->GetPacks() : emptyPacks;
    const int packCount = manager ? manager->GetPackCount() : 0;
    const auto& lastUpdated = manager ? manager->GetLastUpdated() : emptyString;
    const bool scraping = manager && manager->IsScrapingInProgress();

    // Sync selection from Quick Picks (Single Source of Truth)
    if (plugin_->settingsSync) {
        std::string currentQuickPick = plugin_->settingsSync->GetQuickPicksSelectedCode();
        if (currentQuickPick != lastQuickPicksSelected) {
            if (!currentQuickPick.empty()) {
                selectedPackCode = currentQuickPick;
            }
            lastQuickPicksSelected = currentQuickPick;
        }
    }

    // ===== HEADER SECTION =====
    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Training Pack Browser");
    ImGui::Spacing();

    // Status line: pack count, last updated, auto-load, and buttons on same row
    if (packCount > 0) {
        ImGui::Text("Loaded: %d packs", packCount);
        ImGui::SameLine();
        ImGui::TextColored(UI::TrainingPackUI::LAST_UPDATED_TEXT_COLOR, " | Last updated: %s", lastUpdated.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No packs loaded - click 'Update Pack List' to download");
    }

    // Control buttons (same line with spacing)
    ImGui::SameLine(0.0f, 20.0f);
    if (scraping) {
        ImGui::TextColored(UI::TrainingPackUI::SCRAPING_STATUS_TEXT_COLOR, "Updating...");
    } else {
        if (ImGui::Button("Update Pack List")) {
            plugin_->UpdateTrainingPackList();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Download latest training packs from online source (~2-3 minutes)");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload Cache")) {
        plugin_->LoadTrainingPacksFromFile(plugin_->GetTrainingPacksPath());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reload packs from cached json file");
    }

    // Load Now button (Immediate load)
    ImGui::SameLine();
    bool hasSelection = !selectedPackCode.empty();
    if (!hasSelection) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button("LOAD NOW") && hasSelection) {
        LoadPackImmediately(selectedPackCode);
    }
    if (!hasSelection) {
        ImGui::PopStyleVar();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(hasSelection ? "Immediately load the selected pack" : "Select a pack first");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Early return if no packs loaded
    if (packs.empty()) {
        ImGui::TextWrapped("No packs available. Click 'Scrape Packs' to download the training pack database, or add your own custom packs below.");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ===== ADD CUSTOM PACK SECTION =====
    RenderCustomPackForm();

    ImGui::Separator();
    ImGui::Spacing();

    // Early return if no packs loaded
    if (packs.empty()) {
        ImGui::TextWrapped("No packs available. Click 'Scrape Packs' to download the training pack database, or add your own custom packs above.");
        ImGui::End();
        return;
    }

    // ===== FILTER & SEARCH CONTROLS =====
    ImGui::TextUnformatted("Search & Filters:");
    ImGui::Spacing();

    bool filtersChanged = (strcmp(packSearchText, lastSearchText) != 0) ||
                          (packDifficultyFilter != lastDifficultyFilter) ||
                          (packTagFilter != lastTagFilter) ||
                          (packMinShots != lastMinShots) ||
                          (packSortColumn != lastSortColumn) ||
                          (packSortAscending != lastSortAscending) ||
                          (packVideoFilter != lastVideoFilter);

    // Fixed widths for filter controls
    // Search box
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("##search", packSearchText, IM_ARRAYSIZE(packSearchText))) {
        filtersChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Search by pack name, creator, or tag");
    }

    // Difficulty filter
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    const char* difficulties[] = {"All", "Unranked", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Champion", "Grand Champion", "Supersonic Legend"};
    if (ImGui::BeginCombo("##difficulty", packDifficultyFilter.c_str())) {
        for (int i = 0; i < IM_ARRAYSIZE(difficulties); i++) {
            bool selected = (packDifficultyFilter == difficulties[i]);
            if (ImGui::Selectable(difficulties[i], selected)) {
                packDifficultyFilter = difficulties[i];
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filter by difficulty level");
    }

    // Shot count range filter
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderInt("Min Shots", &packMinShots, UI::TrainingPackUI::FILTER_MIN_SHOTS_MIN, UI::TrainingPackUI::FILTER_MIN_SHOTS_MAX)) {
        filtersChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Minimum number of shots in pack");
    }

    // Tag filter dropdown (second row)
    ImGui::SetNextItemWidth(UI::TrainingPackUI::TAG_FILTER_DROPDOWN_WIDTH);

    bool packsSourceChanged = (lastPackCount != packCount);

    if (!tagsInitialized || packsSourceChanged) {
        if (manager) {
            manager->BuildAvailableTags(availableTags);
        } else {
            availableTags.clear();
            availableTags.push_back("All Tags");
        }
        tagsInitialized = true;
        lastPackCount = packCount;
    }

    std::string displayTag = packTagFilter.empty() ? "All Tags" : packTagFilter;
    if (ImGui::BeginCombo("##tagfilter", displayTag.c_str())) {
        for (const auto& tag : availableTags) {
            bool selected = (tag == displayTag);
            if (ImGui::Selectable(tag.c_str(), selected)) {
                packTagFilter = (tag == "All Tags") ? "" : tag;
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filter by tag");
    }

    ImGui::SameLine();

    // Video filter checkbox
    if (ImGui::Checkbox("Has Video", &packVideoFilter)) {
        filtersChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show only packs with video tutorials");
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Filters")) {
        packSearchText[0] = '\0';
        packDifficultyFilter = "All";
        packTagFilter = "";
        packMinShots = 0;
        packMaxShots = 100;
        packVideoFilter = false;
        filtersChanged = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ===== FILTERED & SORTED PACK LIST (cached) =====

    // Rebuild filtered list only when needed
    if (filtersChanged || packsSourceChanged || !packListInitialized) {
        if (manager) {
            manager->FilterAndSortPacks(packSearchText, packDifficultyFilter, packTagFilter,
                packMinShots, packVideoFilter, packSortColumn, packSortAscending, filteredPacks);
        } else {
            filteredPacks.clear();
        }

        // Update cached filter state
        strncpy_s(lastSearchText, packSearchText, sizeof(lastSearchText) - 1);
        lastDifficultyFilter = packDifficultyFilter;
        lastTagFilter = packTagFilter;
        lastMinShots = packMinShots;
        lastVideoFilter = packVideoFilter;
        lastSortColumn = packSortColumn;
        lastSortAscending = packSortAscending;

        // Flag to recalculate column widths
        columnWidthsDirty = true;
        packListInitialized = true;
    }

    // Detect if user is searching by code (contains digits)
    bool showCodeColumn = ShouldShowCodeColumn();
    int activeColumnCount = showCodeColumn ? 6 : 5;

    // Display filtered count
    ImGui::Text("Showing %d of %d packs", (int)filteredPacks.size(), packCount);
    ImGui::Spacing();

    // ===== ACTION BAR =====
    ImGui::Separator();

    // Render browser status message
    browserStatus.Render(ImGui::GetIO().DeltaTime);
    if (browserStatus.IsVisible()) ImGui::Spacing();

    {
        bool hasSelection = !selectedPackCode.empty();
        
        // Find selected pack data
        const TrainingEntry* selectedPack = nullptr;
        if (hasSelection) {
            for (const auto& pack : filteredPacks) {
                if (pack.code == selectedPackCode) {
                    selectedPack = &pack;
                    break;
                }
            }
        }

        // Delete (Custom only)
        if (hasSelection) {
             if (ImGui::Button("Delete Custom Pack")) {
                 if (plugin_->trainingPackMgr) {
                     plugin_->trainingPackMgr->DeletePack(selectedPackCode);
                     browserStatus.ShowSuccess("Deleted custom pack", 3.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                     selectedPackCode = "";
                 }
             }
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Button("Delete Custom Pack");
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();

        // Clear Selection
        if (hasSelection) {
            if (ImGui::Button("Clear Selection")) {
                selectedPackCode = "";
            }
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Button("Clear Selection");
            ImGui::PopStyleVar();
        }
    }

    // ===== TABLE WITH RESIZABLE COLUMNS & FROZEN HEADER =====
    ImGui::Separator();

    // Only set initial column widths once, or when window width changes significantly
    // This allows users to manually resize columns by dragging
    float currentWindowWidth = ImGui::GetWindowContentRegionWidth();
    bool windowResized = std::abs(currentWindowWidth - lastWindowWidth) > 50.0f;

    if (!columnWidthsInitialized || windowResized || columnWidthsDirty) {
        CalculateOptimalColumnWidths();
        columnWidthsInitialized = true;
        columnWidthsDirty = false;
        lastWindowWidth = currentWindowWidth;
    }

    // ===== FROZEN HEADER ROW =====
    ImGui::Columns(activeColumnCount, "PackColumns_Header", true);

    for (int i = 0; i < activeColumnCount && i < (int)columnWidths.size(); i++) {
        ImGui::SetColumnWidth(i, columnWidths[i]);
    }

    // Name column header (Sort ID 0)
    if (SortableColumnHeader("Name", 0, packSortColumn, packSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Code column header (only shown when searching by code)
    if (showCodeColumn) {
        ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Code");
        ImGui::NextColumn();
    }

    // Difficulty column header (Sort ID 2)
    if (SortableColumnHeader("Difficulty", 2, packSortColumn, packSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Shots column header (Sort ID 3)
    if (SortableColumnHeader("Shots", 3, packSortColumn, packSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Likes column header (Sort ID 4)
    if (SortableColumnHeader("Likes", 4, packSortColumn, packSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Plays column header (Sort ID 5)
    if (SortableColumnHeader("Plays", 5, packSortColumn, packSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::Separator();

    // ===== SCROLLABLE PACK ROWS =====
    ImGui::BeginChild("PackTable", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::Columns(activeColumnCount, "PackColumns_Body", true);

    for (int i = 0; i < activeColumnCount && i < (int)columnWidths.size(); i++) {
        ImGui::SetColumnWidth(i, columnWidths[i]);
    }

    ImGuiListClipper clipper;
    clipper.Begin((int)filteredPacks.size());

    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            const auto& pack = filteredPacks[row];

            // Name column with Selection Logic
            bool isSelected = (selectedPackCode == pack.code);
            ImGui::PushID(pack.code.c_str());

            // Play Button (if video exists)
            bool videoClicked = false;
            if (!pack.videoUrl.empty()) {
                if (youtubeIcon && youtubeIcon->IsLoadedForImGui()) {
                    // Use Image with InvisibleButton overlay to remove border
                    // Use text line height instead of frame height for tighter fit
                    float iconSize = ImGui::GetTextLineHeight();
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

                    ImGui::Image(youtubeIcon->GetImGuiTex(), ImVec2(iconSize, iconSize));

                    // Overlay invisible button for click detection (form-fit to icon)
                    ImGui::SetCursorScreenPos(cursorPos);
                    if (ImGui::InvisibleButton("##youtube", ImVec2(iconSize, iconSize))) {
                        videoClicked = true;
                    }
                } else {
                    if (ImGui::ArrowButton("##play", ImGuiDir_Right)) {
                        videoClicked = true;
                    }
                }

                if (videoClicked) {
                    ShellExecuteA(NULL, "open", pack.videoUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Watch Preview");
                
                ImGui::SetItemAllowOverlap();
                ImGui::SameLine();
            } else {
                // Indent to align with packs that have buttons (approximate width of button + Spacing)
                ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), 0));
                ImGui::SameLine();
            }

            // SpanAllColumns allows clicking anywhere in the row
            if (ImGui::Selectable(pack.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns) && !videoClicked) {
                selectedPackCode = pack.code;
                lastSelectedRowIndex = row;
                ImGui::OpenPopup("PackActionPopup");
            }

            if (ImGui::BeginPopup("PackActionPopup")) {
                const auto& trainingPacks = manager ? manager->GetPacks() : emptyPacks;
                auto it = std::find_if(trainingPacks.begin(), trainingPacks.end(), 
                    [&](const TrainingEntry& e) { return e.code == selectedPackCode; });

                if (it != trainingPacks.end()) {
                    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "%s", it->name.c_str());
                    ImGui::Separator();

                    if (ImGui::Selectable("Set Post-Match")) {
                        plugin_->settingsSync->SetQuickPicksSelected(selectedPackCode);
                        plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(selectedPackCode);
                        // Also sync with current training code for consistency
                        plugin_->settingsSync->SetCurrentTrainingCode(selectedPackCode);
                        plugin_->cvarManager->getCvar("suitespot_current_training_code").setValue(selectedPackCode);
                        browserStatus.ShowSuccess("Post-Match set: " + it->name, 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                    }

                    if (ImGui::Selectable("Load Now")) {
                        if (plugin_->usageTracker) plugin_->usageTracker->IncrementLoadCount(selectedPackCode);
                        std::string code = selectedPackCode;
                        std::string name = it->name;
                        SuiteSpot* p = plugin_;
                        p->gameWrapper->SetTimeout([p, code, name](GameWrapper* gw) {
                            p->cvarManager->executeCommand("load_training " + code);
                            LOG("SuiteSpot: Loading training pack from browser: " + name);
                        }, 0.0f);
                        if (plugin_->cvarManager) {
                            plugin_->cvarManager->executeCommand("togglemenu settings");
                        }
                    }
                }
                ImGui::EndPopup();
            }

            // === DRAG SOURCE for drag-and-drop to bag manager ===
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                // Payload: pack code as null-terminated string
                ImGui::SetDragDropPayload("TRAINING_PACK_CODE", pack.code.c_str(), pack.code.length() + 1);

                // Preview during drag
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Dragging: %s", pack.name.c_str());

                ImGui::EndDragDropSource();
            }

            // Right-click context menu for auto-load
            if (ImGui::BeginPopupContextItem(("PackContext_" + pack.code).c_str())) {
                ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "%s", pack.name.c_str());
                ImGui::Separator();

                // Set as Auto-Load option
                if (ImGui::Selectable("Set as Auto-Load")) {
                    plugin_->settingsSync->SetCurrentTrainingCode(pack.code);
                    plugin_->cvarManager->getCvar("suitespot_current_training_code").setValue(pack.code);
                    browserStatus.ShowSuccess("Auto-Load set: " + pack.name, 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
            if (ImGui::IsItemHovered()) {
                std::string tooltip = "";
                if (!pack.staffComments.empty()) tooltip += pack.staffComments + "\n";
                if (!pack.creator.empty()) tooltip += "Creator: " + pack.creator + "\n";
                if (!pack.tags.empty()) {
                    tooltip += "Tags: ";
                    for (size_t i = 0; i < pack.tags.size(); i++) {
                        if (i > 0) tooltip += ", ";
                        tooltip += pack.tags[i];
                    }
                }
                if (!tooltip.empty()) {
                    ImVec2 mPos = ImGui::GetMousePos();
                    ImGui::SetNextWindowPos(ImVec2(mPos.x + 20, mPos.y + 20));
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(450.0f);
                    ImGui::TextUnformatted(tooltip.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
            ImGui::NextColumn();

            // Code column (only shown when searching by code)
            if (showCodeColumn) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", pack.code.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Pack Code: %s", pack.code.c_str());
                }
                ImGui::NextColumn();
            }

            // Difficulty column with color coding
            ImVec4 diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_UNRANKED_COLOR;
            std::string displayDifficulty = pack.difficulty;
            
            if (displayDifficulty.empty() || displayDifficulty == "Unknown" || displayDifficulty == "All") {
                displayDifficulty = "Unranked";
            }

            if (displayDifficulty == "Bronze") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_BRONZE_COLOR;
            else if (displayDifficulty == "Silver") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_SILVER_COLOR;
            else if (displayDifficulty == "Gold") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_GOLD_COLOR;
            else if (displayDifficulty == "Platinum") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_PLATINUM_COLOR;
            else if (displayDifficulty == "Diamond") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_DIAMOND_COLOR;
            else if (displayDifficulty == "Champion") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_CHAMPION_COLOR;
            else if (displayDifficulty == "Grand Champion") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_GRAND_CHAMPION_COLOR;
            else if (displayDifficulty == "Supersonic Legend") diffColor = UI::TrainingPackUI::DIFFICULTY_BADGE_SUPERSONIC_LEGEND_COLOR;
            
            ImGui::TextColored(diffColor, "%s", displayDifficulty.c_str());
            ImGui::NextColumn();

            // Shots column
            ImGui::Text("%d", pack.shotCount);
            ImGui::NextColumn();

            // Likes column
            ImGui::Text("%d", pack.likes);
            ImGui::NextColumn();

            // Plays column
            ImGui::Text("%d", pack.plays);
            ImGui::NextColumn();
        }
    }

    // End columns
    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::End();
}



bool TrainingPackUI::ValidatePackCode(const char* code) const {
    if (strlen(code) != UI::TrainingPackUI::PACK_CODE_EXPECTED_LENGTH) return false;
    for (int i = 0; i < UI::TrainingPackUI::PACK_CODE_EXPECTED_LENGTH; i++) {
        if (i == UI::TrainingPackUI::PACK_CODE_DASH_POSITION_1 || i == UI::TrainingPackUI::PACK_CODE_DASH_POSITION_2 || i == UI::TrainingPackUI::PACK_CODE_DASH_POSITION_3) {
            if (code[i] != '-') return false;
        }
        else {
            if (!isalnum(static_cast<unsigned char>(code[i]))) return false;
        }
    }
    return true;
}

void TrainingPackUI::ClearCustomPackForm() {
    customPackCode[0] = '\0';
    customPackName[0] = '\0';
    customPackCreator[0] = '\0';
    customPackDifficulty = 0;
    customPackShotCount = 10;
    customPackTags[0] = '\0';
    customPackNotes[0] = '\0';
    customPackVideoUrl[0] = '\0';
    customPackStatus.Clear();
}

void TrainingPackUI::RenderCustomPackForm() {
    if (ImGui::CollapsingHeader("Add Custom Pack")) {
        ImGui::Indent(UI::TrainingPackUI::CUSTOM_PACK_FORM_INDENT);
        ImGui::Spacing();

        customPackStatus.Render(ImGui::GetIO().DeltaTime);
        if (customPackStatus.IsVisible()) {
            ImGui::Spacing();
        }

        ImGui::TextUnformatted("Code *");
        ImGui::SameLine();
        ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "(XXXX-XXXX-XXXX-XXXX)");
        ImGui::SetNextItemWidth(UI::TrainingPackUI::CUSTOM_PACK_CODE_INPUT_WIDTH);
        if (ImGui::InputText("##customcode", customPackCode, IM_ARRAYSIZE(customPackCode))) {
            std::string raw;
            for (int i = 0; customPackCode[i]; i++) {
                char c = customPackCode[i];
                if (isalnum(static_cast<unsigned char>(c))) {
                    raw += static_cast<char>(toupper(static_cast<unsigned char>(c)));
                }
            }
            if (raw.length() > UI::TrainingPackUI::PACK_CODE_RAW_MAX_LENGTH) raw = raw.substr(0, UI::TrainingPackUI::PACK_CODE_RAW_MAX_LENGTH);
            std::string formatted;
            for (size_t i = 0; i < raw.length(); i++) {
                if (i > 0 && i % 4 == 0) formatted += '-';
                formatted += raw[i];
            }
            strncpy_s(customPackCode, formatted.c_str(), sizeof(customPackCode) - 1);
        }

        ImGui::TextUnformatted("Name *");
        ImGui::SetNextItemWidth(UI::TrainingPackUI::CUSTOM_PACK_NAME_INPUT_WIDTH);
        ImGui::InputText("##customname", customPackName, IM_ARRAYSIZE(customPackName));

        ImGui::TextUnformatted("Creator");
        ImGui::SetNextItemWidth(UI::TrainingPackUI::CUSTOM_PACK_CREATOR_INPUT_WIDTH);
        ImGui::InputText("##customcreator", customPackCreator, IM_ARRAYSIZE(customPackCreator));

        ImGui::TextUnformatted("Difficulty");
        ImGui::SetNextItemWidth(UI::TrainingPackUI::CUSTOM_PACK_DIFFICULTY_DROPDOWN_WIDTH);
        const char* difficulties[] = {"Unranked", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Champion", "Grand Champion", "Supersonic Legend"};
        ImGui::Combo("##customdifficulty", &customPackDifficulty, difficulties, IM_ARRAYSIZE(difficulties));

        ImGui::TextUnformatted("Shot Count");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderInt("##customshots", &customPackShotCount, UI::TrainingPackUI::CUSTOM_PACK_SHOTS_MIN, UI::TrainingPackUI::CUSTOM_PACK_SHOTS_MAX);

        ImGui::TextUnformatted("Tags");
        ImGui::SameLine();
        ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "(comma-separated)");
        ImGui::SetNextItemWidth(UI::TrainingPackUI::CUSTOM_PACK_TAGS_INPUT_WIDTH);
        ImGui::InputText("##customtags", customPackTags, IM_ARRAYSIZE(customPackTags));

        ImGui::TextUnformatted("Notes");
        ImGui::InputTextMultiline("##customnotes", customPackNotes, IM_ARRAYSIZE(customPackNotes), ImVec2(UI::TrainingPackUI::CUSTOM_PACK_NOTES_INPUT_WIDTH, UI::TrainingPackUI::CUSTOM_PACK_NOTES_INPUT_HEIGHT));

        ImGui::TextUnformatted("Video URL");
        ImGui::SetNextItemWidth(UI::TrainingPackUI::CUSTOM_PACK_VIDEO_URL_INPUT_WIDTH);
        ImGui::InputText("##customvideo", customPackVideoUrl, IM_ARRAYSIZE(customPackVideoUrl));

        ImGui::Spacing();

        if (ImGui::Button("Add Pack", ImVec2(UI::TrainingPackUI::CUSTOM_PACK_ADD_BUTTON_WIDTH, UI::TrainingPackUI::CUSTOM_PACK_ADD_BUTTON_HEIGHT))) {
            customPackStatus.Clear();
            if (strlen(customPackCode) == 0) {
                customPackStatus.ShowError("Pack code is required");
            }
            else if (!ValidatePackCode(customPackCode)) {
                customPackStatus.ShowError("Invalid code format. Expected: XXXX-XXXX-XXXX-XXXX");
            }
            else if (strlen(customPackName) == 0) {
                customPackStatus.ShowError("Pack name is required");
            }
            else {
                TrainingEntry pack;
                pack.code = customPackCode;
                pack.name = customPackName;
                pack.creator = strlen(customPackCreator) > 0 ? customPackCreator : "Unknown";
                const char* difficultyNames[] = {"Unranked", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Champion", "Grand Champion", "Supersonic Legend"};
                pack.difficulty = difficultyNames[customPackDifficulty];
                pack.shotCount = customPackShotCount;
                if (strlen(customPackTags) > 0) {
                    std::string tagsStr = customPackTags;
                    size_t start = 0;
                    size_t end = tagsStr.find(',');
                    while (end != std::string::npos) {
                        std::string tag = tagsStr.substr(start, end - start);
                        size_t first = tag.find_first_not_of(" \t");
                        size_t last = tag.find_last_not_of(" \t");
                        if (first != std::string::npos) {
                            pack.tags.push_back(tag.substr(first, last - first + 1));
                        }
                        start = end + 1;
                        end = tagsStr.find(',', start);
                    }
                    std::string tag = tagsStr.substr(start);
                    size_t first = tag.find_first_not_of(" \t");
                    size_t last = tag.find_last_not_of(" \t");
                    if (first != std::string::npos) {
                        pack.tags.push_back(tag.substr(first, last - first + 1));
                    }
                }
                pack.staffComments = customPackNotes;
                pack.videoUrl = customPackVideoUrl;
                pack.source = "custom";
                pack.isModified = false;
                if (plugin_->trainingPackMgr) {
                    if (plugin_->trainingPackMgr->AddCustomPack(pack)) {
                        customPackStatus.ShowSuccess("Pack added successfully!");
                        ClearCustomPackForm();
                        LOG("SuiteSpot: Added custom pack: " + pack.name);
                    } else {
                        customPackStatus.ShowError("Pack with this code already exists");
                    }
                } else {
                    customPackStatus.ShowError("Pack manager not initialized");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(UI::TrainingPackUI::CUSTOM_PACK_CLEAR_BUTTON_WIDTH, UI::TrainingPackUI::CUSTOM_PACK_CLEAR_BUTTON_HEIGHT))) {
            ClearCustomPackForm();
        }
        ImGui::Spacing();
        ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "* Required fields");
        ImGui::Unindent(UI::TrainingPackUI::CUSTOM_PACK_FORM_INDENT);
        ImGui::Spacing();
    }
}

bool TrainingPackUI::ShouldShowCodeColumn() const {
    for (size_t i = 0; packSearchText[i] != '\0'; i++) {
        if (std::isdigit(static_cast<unsigned char>(packSearchText[i]))) {
            return true;
        }
    }
    return false;
}

void TrainingPackUI::CalculateOptimalColumnWidths() {
    float availWidth = ImGui::GetWindowContentRegionWidth();
    int columnCount = ShouldShowCodeColumn() ? 6 : 5;
    columnWidths.assign(columnCount, 0.0f);

    if (ShouldShowCodeColumn()) {
        // Name (35%), Code (15%), Difficulty (20%), Shots (10%), Likes (10%), Plays (10%)
        columnWidths[0] = availWidth * 0.35f;
        columnWidths[1] = availWidth * 0.15f;
        columnWidths[2] = availWidth * 0.20f;
        columnWidths[3] = availWidth * 0.10f;
        columnWidths[4] = availWidth * 0.10f;
        columnWidths[5] = availWidth * 0.10f;
    } else {
        // Name (45%), Difficulty (25%), Shots (10%), Likes (10%), Plays (10%)
        columnWidths[0] = availWidth * 0.45f;
        columnWidths[1] = availWidth * 0.25f;
        columnWidths[2] = availWidth * 0.10f;
        columnWidths[3] = availWidth * 0.10f;
        columnWidths[4] = availWidth * 0.10f;
    }

    // Apply minimum widths to ensure readability
    for (int i = 0; i < columnCount; i++) {
        if (i == 0 && columnWidths[i] < 150.0f) columnWidths[i] = 150.0f;
        else if (columnWidths[i] < 60.0f) columnWidths[i] = 60.0f;
    }
}

std::string TrainingPackUI::GetMenuName() {
    return "suitespot_browser";
}

std::string TrainingPackUI::GetMenuTitle() {
    return "SuiteSpot Training Browser";
}

void TrainingPackUI::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool TrainingPackUI::ShouldBlockInput() {
    if (!isWindowOpen_) {
        return false;  // Window closed → no blocking
    }

    // ============================================================================
    // INPUT BLOCKING STRATEGY
    // ============================================================================
    //
    // The default PluginWindowBase::ShouldBlockInput() blocks ALL application
    // input whenever ImGui wants mouse or keyboard input. This is too aggressive
    // and prevents multi-window interactions like drag-and-drop.
    //
    // Our selective approach:
    // 1. Allow drag-and-drop between browser and bag manager (modal popups)
    // 2. Block only when user is actively typing in text fields
    // 3. Allow normal mouse interactions (clicking, hovering, scrolling)
    //
    // This enables the drag-and-drop UX while maintaining text input safety.
    // Based on: docs/Examples/ManagingMultipleWindows.md:163-180
    // ============================================================================

    ImGuiIO& io = ImGui::GetIO();

    // Block when actively typing in text fields (search box, custom pack form)
    // This prevents game commands from firing while typing
    if (io.WantTextInput && ImGui::IsAnyItemActive()) {
        return true;
    }

    // Allow normal mouse interaction without blocking game input
    // This includes drag-and-drop operations between browser and bag manager modal
    // Note: We intentionally DON'T block for popups/modals because the Bag Manager modal
    // needs to allow drag operations from the parent browser window (same ImGui context)
    return false;
}

bool TrainingPackUI::IsActiveOverlay() {
    return isWindowOpen_;
}

void TrainingPackUI::OnOpen() {
    isWindowOpen_ = true;
    needsFocusOnNextRender_ = true;  // Bring window to front on next render
}

void TrainingPackUI::OnClose() {
    isWindowOpen_ = false;
}

bool TrainingPackUI::IsOpen() {
    return isWindowOpen_;
}

void TrainingPackUI::SetOpen(bool open) {
    isWindowOpen_ = open;
}

void TrainingPackUI::LoadPackImmediately(const std::string& packCode) {
    if (packCode.empty() || !plugin_) return;

    // Increment usage stats
    if (plugin_->usageTracker) {
        plugin_->usageTracker->IncrementLoadCount(packCode);
    }

    // Find pack name for logging
    std::string packName = packCode;
    const auto* manager = plugin_->trainingPackMgr.get();
    if (manager) {
        for (const auto& pack : manager->GetPacks()) {
            if (pack.code == packCode) {
                packName = pack.name;
                break;
            }
        }
    }

    // Execute load immediately (0 delay)
    SuiteSpot* p = plugin_;
    std::string code = packCode;
    p->gameWrapper->SetTimeout([p, code, packName](GameWrapper* gw) {
        p->cvarManager->executeCommand("load_training " + code);
        LOG("SuiteSpot: Loading training pack immediately: {}", packName);
    }, 0.0f);

    browserStatus.ShowSuccess("Loading: " + packName, 2.0f,
        UI::StatusMessage::DisplayMode::TimerWithFade);

    if (plugin_->cvarManager) {
        plugin_->cvarManager->executeCommand("togglemenu settings");
    }
}
