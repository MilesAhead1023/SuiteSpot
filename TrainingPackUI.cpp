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
#include "bakkesmod/wrappers/http/HttpWrapper.h"
#include "IMGUI/SuiteSpotIcons.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Sortable column header that shows asc/desc indicator and handles click
bool SortableColumnHeader(const char* label, int columnIndex, int& currentSortColumn, bool& sortAscending)
{
    char buffer[256];
    if (currentSortColumn == columnIndex)
        snprintf(buffer, sizeof(buffer), "%s %s", label, sortAscending ? "(asc)" : "(desc)");
    else
        snprintf(buffer, sizeof(buffer), "%s", label);

    bool clicked = ImGui::Selectable(buffer, currentSortColumn == columnIndex, ImGuiSelectableFlags_DontClosePopups);
    if (clicked) {
        if (currentSortColumn == columnIndex)
            sortAscending = !sortAscending;
        else {
            currentSortColumn = columnIndex;
            sortAscending = true;
        }
    }
    return clicked;
}

// Draw a small colored text badge (no button, just colored text in brackets)
void DrawTagBadge(const char* tag)
{
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::TAG_TEXT_COLOR);
    ImGui::Text("[%s]", tag);
    ImGui::PopStyleColor();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// DifficultyColor (static helper used in both panels)
// ─────────────────────────────────────────────────────────────────────────────
/*static*/ ImVec4 TrainingPackUI::DifficultyColor(const std::string& difficulty)
{
    if (difficulty == "Bronze") return UI::TrainingPackUI::DIFFICULTY_BADGE_BRONZE_COLOR;
    if (difficulty == "Silver") return UI::TrainingPackUI::DIFFICULTY_BADGE_SILVER_COLOR;
    if (difficulty == "Gold") return UI::TrainingPackUI::DIFFICULTY_BADGE_GOLD_COLOR;
    if (difficulty == "Platinum") return UI::TrainingPackUI::DIFFICULTY_BADGE_PLATINUM_COLOR;
    if (difficulty == "Diamond") return UI::TrainingPackUI::DIFFICULTY_BADGE_DIAMOND_COLOR;
    if (difficulty == "Champion") return UI::TrainingPackUI::DIFFICULTY_BADGE_CHAMPION_COLOR;
    if (difficulty == "Grand Champion") return UI::TrainingPackUI::DIFFICULTY_BADGE_GRAND_CHAMPION_COLOR;
    if (difficulty == "Supersonic Legend") return UI::TrainingPackUI::DIFFICULTY_BADGE_SUPERSONIC_LEGEND_COLOR;
    return UI::TrainingPackUI::DIFFICULTY_BADGE_UNRANKED_COLOR;
}

// ─────────────────────────────────────────────────────────────────────────────
// ExtractYouTubeId
// ─────────────────────────────────────────────────────────────────────────────
/*static*/ std::string TrainingPackUI::ExtractYouTubeId(const std::string& url)
{
    if (url.empty()) return {};

    // youtu.be/{ID}
    auto pos = url.find("youtu.be/");
    if (pos != std::string::npos) {
        pos += 9;
        auto end = url.find_first_of("?&", pos);
        return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    // youtube.com/shorts/{ID}
    pos = url.find("/shorts/");
    if (pos != std::string::npos) {
        pos += 8;
        auto end = url.find_first_of("?&", pos);
        return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    // youtube.com/watch?v={ID}
    pos = url.find("v=");
    if (pos != std::string::npos) {
        pos += 2;
        auto end = url.find_first_of("&", pos);
        return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// FetchThumbnailForSelected
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::FetchThumbnailForSelected()
{
    if (selectedPackCode.empty() || selectedPackCode == lastFetchedThumbnailCode_) return;
    lastFetchedThumbnailCode_ = selectedPackCode;

    // Find the entry in filteredPacks
    TrainingEntry* entry = nullptr;
    for (auto& p : filteredPacks) {
        if (p.code == selectedPackCode) {
            entry = &p;
            break;
        }
    }
    if (!entry || entry->videoUrl.empty()) return;
    if (entry->isThumbnailRequested) return;

    std::string videoId = ExtractYouTubeId(entry->videoUrl);
    if (videoId.empty()) return;

    entry->isThumbnailRequested = true;

    // Check disk cache first
    std::filesystem::path cachedPath = thumbnailCacheDir_ / (videoId + ".jpg");
    if (std::filesystem::exists(cachedPath)) {
        entry->thumbnailImage = std::make_shared<ImageWrapper>(cachedPath.string(), false, true);
        return;
    }

    // Download thumbnail
    std::string thumbUrl = "https://img.youtube.com/vi/" + videoId + "/mqdefault.jpg";
    std::filesystem::create_directories(thumbnailCacheDir_);

    // Capture by value — entry ptr may be invalidated on next filter rebuild
    std::string cachePath = cachedPath.string();
    std::string code = selectedPackCode;
    TrainingPackUI* self = this;

    CurlRequest req;
    req.url = thumbUrl;
    HttpWrapper::SendCurlRequest(req, [self, cachePath, code](int httpCode, char* data, size_t size) {
        if (httpCode != 200 || size == 0) return;

        // Write to disk
        std::ofstream f(cachePath, std::ios::binary);
        if (!f) return;
        f.write(data, static_cast<std::streamsize>(size));
        f.close();

        // Load via ImageWrapper — find entry again (filteredPacks may have changed)
        for (auto& p : self->filteredPacks) {
            if (p.code == code) {
                p.thumbnailImage = std::make_shared<ImageWrapper>(cachePath, false, true);
                break;
            }
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / PluginWindow boilerplate
// ─────────────────────────────────────────────────────────────────────────────
TrainingPackUI::TrainingPackUI(SuiteSpot* plugin) : plugin_(plugin)
{
    thumbnailCacheDir_ = plugin_->GetDataRoot() / "SuiteSpot" / "ThumbnailCache";
}

std::string TrainingPackUI::GetMenuName()
{
    return "suitespot_browser";
}
std::string TrainingPackUI::GetMenuTitle()
{
    return "SuiteSpot Training Browser";
}
void TrainingPackUI::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool TrainingPackUI::ShouldBlockInput()
{
    if (!isWindowOpen_) return false;
    ImGuiIO& io = ImGui::GetIO();
    return io.WantTextInput && ImGui::IsAnyItemActive();
}

bool TrainingPackUI::IsActiveOverlay()
{
    return isWindowOpen_;
}

void TrainingPackUI::OnOpen()
{
    isWindowOpen_ = true;
    needsFocusOnNextRender_ = true;
}

void TrainingPackUI::OnClose()
{
    isWindowOpen_ = false;
}
bool TrainingPackUI::IsOpen()
{
    return isWindowOpen_;
}
void TrainingPackUI::SetOpen(bool open)
{
    isWindowOpen_ = open;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render — main entry point
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::Render()
{
    if (!isWindowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);

    if (needsFocusOnNextRender_) {
        ImGui::SetNextWindowFocus();
        needsFocusOnNextRender_ = false;
    }

    // ── Style push (11 vars, 17 colors) ──────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::INTERACTIVE_FRAME_BORDER_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, UI::INTERACTIVE_FRAME_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::FRAME_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UI::ITEM_SPACING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, UI::CHILD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, UI::CHILD_BORDER_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, UI::TAB_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, UI::GRAB_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, UI::GRAB_MIN_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, UI::SCROLLBAR_ROUNDING);

    ImGui::PushStyleColor(ImGuiCol_Border, UI::INTERACTIVE_BORDER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button, UI::BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::BUTTON_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::BUTTON_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, UI::FRAME_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, UI::FRAME_BG_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UI::FRAME_BG_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, UI::CHECKMARK_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Header, UI::HEADER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::HEADER_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::HEADER_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Tab, UI::TAB_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, UI::TAB_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabActive, UI::TAB_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused, UI::TAB_UNFOCUSED_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, UI::TAB_UNFOCUSED_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::CHILD_BG_COLOR);

    if (!ImGui::Begin(GetMenuTitle().c_str(), &isWindowOpen_)) {
        ImGui::PopStyleColor(17);
        ImGui::PopStyleVar(11);
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(UI::FONT_SCALE);

    const auto* manager = plugin_->trainingPackMgr.get();
    static const std::vector<TrainingEntry> emptyPacks;
    const auto& packs = manager ? manager->GetPacks() : emptyPacks;
    const int packCount = manager ? manager->GetPackCount() : 0;
    const auto& lastUpdated = manager ? manager->GetLastUpdated() : std::string{};
    const bool scraping = manager && manager->IsScrapingInProgress();

    // ── Sync selection from Quick Picks ──────────────────────────────────────
    if (plugin_->settingsSync) {
        std::string qp = plugin_->settingsSync->GetQuickPicksSelectedCode();
        if (qp != lastQuickPicksSelected) {
            if (!qp.empty()) selectedPackCode = qp;
            lastQuickPicksSelected = qp;
        }
    }

    // ── Window header row ─────────────────────────────────────────────────────
    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Training Pack Browser");
    ImGui::SameLine();
    if (packCount > 0) {
        ImGui::TextDisabled("  %d packs", packCount);
        if (!lastUpdated.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("| Updated: %s", lastUpdated.c_str());
        }
    }

    // Right-aligned control buttons
    float btnW = ImGui::CalcTextSize("Reload Cache").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
    float updateW = ImGui::CalcTextSize("Update Pack List").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - btnW - updateW - ImGui::GetStyle().ItemSpacing.x);

    if (scraping) {
        ImGui::TextColored(UI::TrainingPackUI::SCRAPING_STATUS_TEXT_COLOR, "Updating...");
        ImGui::SameLine();
    } else {
        if (ImGui::Button("Update Pack List")) plugin_->UpdateTrainingPackList();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Download latest training packs (~2-3 minutes)");
        ImGui::SameLine();
    }
    if (ImGui::Button("Reload Cache")) {
        plugin_->LoadTrainingPacksFromFile(plugin_->GetTrainingPacksPath());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reload packs from cached JSON");

    ImGui::Separator();
    ImGui::Spacing();

    if (packs.empty()) {
        ImGui::TextWrapped("No packs available. Click 'Update Pack List' to download the training pack database.");
        ImGui::PopStyleColor(17);
        ImGui::PopStyleVar(11);
        ImGui::End();
        return;
    }

    // ── Filter bar (full width above both panels) ─────────────────────────────
    bool filtersChanged = (strcmp(packSearchText, lastSearchText) != 0) ||
                          (packDifficultyFilter != lastDifficultyFilter) || (packTagFilter != lastTagFilter) ||
                          (packMinShots != lastMinShots) || (packVideoFilter != lastVideoFilter) ||
                          (packSortColumn != lastSortColumn) || (packSortAscending != lastSortAscending);

    // Row 1: search + difficulty + tags + has-video + clear
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputTextWithHint("##search", "Search name, creator, tag...", packSearchText, IM_ARRAYSIZE(packSearchText)))
        filtersChanged = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Search by pack name, creator, or tag");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    static const char* difficulties[] = {"All",      "Unranked", "Bronze",   "Silver",         "Gold",
                                         "Platinum", "Diamond",  "Champion", "Grand Champion", "Supersonic Legend"};
    if (ImGui::BeginCombo("##diff", packDifficultyFilter.c_str())) {
        for (auto& d : difficulties) {
            bool sel = (packDifficultyFilter == d);
            if (ImGui::Selectable(d, sel)) {
                packDifficultyFilter = d;
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter by difficulty");

    ImGui::SameLine();
    bool packsSourceChanged = (lastPackCount != packCount);
    if (!tagsInitialized || packsSourceChanged) {
        if (manager)
            manager->BuildAvailableTags(availableTags);
        else {
            availableTags.clear();
            availableTags.push_back("All Tags");
        }
        tagsInitialized = true;
        lastPackCount = packCount;
    }
    std::string displayTag = packTagFilter.empty() ? "All Tags" : packTagFilter;
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##tags", displayTag.c_str())) {
        for (const auto& tag : availableTags) {
            bool sel = (tag == displayTag);
            if (ImGui::Selectable(tag.c_str(), sel)) {
                packTagFilter = (tag == "All Tags") ? "" : tag;
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter by tag");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::SliderInt("Min Shots##filter", &packMinShots, 0, 50)) filtersChanged = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum shots in pack");

    ImGui::SameLine();
    if (ImGui::Checkbox("Has Video", &packVideoFilter)) filtersChanged = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show only packs with video tutorial links");

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        packSearchText[0] = '\0';
        packDifficultyFilter = "All";
        packTagFilter = "";
        packMinShots = 0;
        packVideoFilter = false;
        filtersChanged = true;
    }

    // Showing count
    ImGui::SameLine();
    ImGui::TextDisabled("(%d shown)", (int)filteredPacks.size());

    ImGui::Spacing();

    // ── Rebuild filtered list when needed ────────────────────────────────────
    if (filtersChanged || packsSourceChanged || !packListInitialized) {
        if (manager) {
            manager->FilterAndSortPacks(packSearchText, packDifficultyFilter, packTagFilter, packMinShots,
                                        packVideoFilter, packSortColumn, packSortAscending, filteredPacks);
        } else {
            filteredPacks.clear();
        }
        strncpy_s(lastSearchText, packSearchText, sizeof(lastSearchText) - 1);
        lastDifficultyFilter = packDifficultyFilter;
        lastTagFilter = packTagFilter;
        lastMinShots = packMinShots;
        lastVideoFilter = packVideoFilter;
        lastSortColumn = packSortColumn;
        lastSortAscending = packSortAscending;
        packListInitialized = true;
    }

    // ── Status bar ───────────────────────────────────────────────────────────
    browserStatus.Render(ImGui::GetIO().DeltaTime);
    if (browserStatus.IsVisible()) ImGui::Spacing();

    // ── Two-panel layout ─────────────────────────────────────────────────────
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::PackBrowserUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::PackBrowserUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup();

    // ─────────────────────────────────────────────────────────────────────────
    // LEFT PANEL: list
    // ─────────────────────────────────────────────────────────────────────────
    if (ImGui::BeginChild("PackList", ImVec2(leftWidth, UI::PackBrowserUI::BROWSER_HEIGHT), true, ImGuiWindowFlags_None)) {
        // Frozen column header row
        ImGui::Columns(3, "PackHdr", true);
        ImGui::SetColumnWidth(0, leftWidth * 0.55f);
        ImGui::SetColumnWidth(1, leftWidth * 0.25f);
        // col 2 = remaining

        if (SortableColumnHeader("Name", 0, packSortColumn, packSortAscending)) filtersChanged = true;
        ImGui::NextColumn();
        if (SortableColumnHeader("Difficulty", 2, packSortColumn, packSortAscending)) filtersChanged = true;
        ImGui::NextColumn();
        if (SortableColumnHeader("Shots", 3, packSortColumn, packSortAscending)) filtersChanged = true;
        ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::Separator();

        // Scrollable rows
        if (ImGui::BeginChild("PackRows", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4.0f), false)) {
            ImGui::Columns(3, "PackBody", true);
            ImGui::SetColumnWidth(0, leftWidth * 0.55f);
            ImGui::SetColumnWidth(1, leftWidth * 0.25f);

            ImGuiListClipper clipper;
            clipper.Begin((int)filteredPacks.size());
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    auto& pack = filteredPacks[row];
                    bool isSelected = (selectedPackCode == pack.code);

                    ImGui::PushID(pack.code.c_str());

                    // Video indicator dot
                    if (!pack.videoUrl.empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), ICON_FA_PLAY);
                        ImGui::SameLine(0, 3.0f);
                    }

                    if (ImGui::Selectable(pack.name.c_str(), isSelected,
                                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                        selectedPackCode = pack.code;
                        FetchThumbnailForSelected();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                    // Scroll to item when it becomes selected
                    if (isSelected && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);

                    // Context menu
                    if (ImGui::BeginPopupContextItem("##ctx")) {
                        ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "%s", pack.name.c_str());
                        ImGui::Separator();
                        if (ImGui::Selectable("Set Post-Match")) {
                            plugin_->settingsSync->SetQuickPicksSelected(pack.code);
                            plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(pack.code);
                            plugin_->settingsSync->SetCurrentTrainingCode(pack.code);
                            plugin_->cvarManager->getCvar("suitespot_current_training_code").setValue(pack.code);
                            browserStatus.ShowSuccess("Post-Match set: " + pack.name, 2.0f,
                                                      UI::StatusMessage::DisplayMode::TimerWithFade);
                        }
                        if (ImGui::Selectable("Load Now")) {
                            LoadPackImmediately(pack.code);
                        }
                        if (!pack.videoUrl.empty() && ImGui::Selectable("Watch Video")) {
                            ShellExecuteA(NULL, "open", pack.videoUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        }
                        ImGui::EndPopup();
                    }

                    // Drag source for bag manager
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("TRAINING_PACK_CODE", pack.code.c_str(), pack.code.length() + 1);
                        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Dragging: %s", pack.name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    ImGui::PopID();
                    ImGui::NextColumn();

                    // Difficulty column
                    std::string diff = pack.difficulty.empty() ? "Unranked" : pack.difficulty;
                    ImGui::TextColored(DifficultyColor(diff), "%s", diff.c_str());
                    ImGui::NextColumn();

                    // Shots column
                    ImGui::Text("%d", pack.shotCount);
                    ImGui::NextColumn();
                }
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();

        // Left panel footer: "+ Add Custom Pack" button
        ImGui::Separator();
        if (ImGui::Button("+ Add Custom Pack", ImVec2(-1.0f, 0.0f))) {
            showAddPackModal_ = true;
            ImGui::OpenPopup("Add Custom Pack");
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ─────────────────────────────────────────────────────────────────────────
    // RIGHT PANEL: detail
    // ─────────────────────────────────────────────────────────────────────────
    if (ImGui::BeginChild("PackDetail", ImVec2(rightWidth, UI::PackBrowserUI::BROWSER_HEIGHT), true,
                          ImGuiWindowFlags_None)) {
        // Find selected pack
        const TrainingEntry* selectedPack = nullptr;
        for (auto& p : filteredPacks) {
            if (p.code == selectedPackCode) {
                selectedPack = &p;
                break;
            }
        }
        RenderDetailPanel(selectedPack);
    }
    ImGui::EndChild();

    ImGui::EndGroup();

    // ── Modal: Add Custom Pack ────────────────────────────────────────────────
    RenderCustomPackModal();

    ImGui::PopStyleColor(17);
    ImGui::PopStyleVar(11);
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// RenderDetailPanel
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::RenderDetailPanel(const TrainingEntry* pack)
{
    if (!pack) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avail.y * 0.40f);
        ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::NO_SELECTION_COLOR);
        float textW = ImGui::CalcTextSize("Select a pack from the list").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - textW) * 0.5f);
        ImGui::TextUnformatted("Select a pack from the list");
        ImGui::PopStyleColor();
        return;
    }

    float panelW = ImGui::GetContentRegionAvail().x;

    // ── Thumbnail ────────────────────────────────────────────────────────────
    float thumbW = std::min(UI::PackBrowserUI::THUMBNAIL_WIDTH, panelW);
    float thumbH = thumbW * (UI::PackBrowserUI::THUMBNAIL_HEIGHT / UI::PackBrowserUI::THUMBNAIL_WIDTH);

    if (pack->thumbnailImage && pack->thumbnailImage->GetImGuiTex()) {
        float offsetX = (panelW - thumbW) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::Image(pack->thumbnailImage->GetImGuiTex(), ImVec2(thumbW, thumbH));
    } else {
        // Placeholder rect with centered text
        ImVec2 p = ImGui::GetCursorScreenPos();
        float offsetX = (panelW - thumbW) * 0.5f;
        p.x += offsetX;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + thumbW, p.y + thumbH), ImColor(35, 38, 48, 255), 4.0f);
        dl->AddRect(p, ImVec2(p.x + thumbW, p.y + thumbH), ImColor(60, 70, 90, 255), 4.0f);
        if (!pack->videoUrl.empty()) {
            const char* msg = pack->isThumbnailRequested ? "Loading..." : "No Preview";
            float tw = ImGui::CalcTextSize(msg).x;
            dl->AddText(ImVec2(p.x + (thumbW - tw) * 0.5f, p.y + thumbH * 0.5f - 7.0f), ImColor(100, 110, 130, 255), msg);
        } else {
            const char* msg = "No Video";
            float tw = ImGui::CalcTextSize(msg).x;
            dl->AddText(ImVec2(p.x + (thumbW - tw) * 0.5f, p.y + thumbH * 0.5f - 7.0f), ImColor(80, 85, 100, 255), msg);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::Dummy(ImVec2(thumbW, thumbH));
    }

    ImGui::Spacing();

    // ── Pack name ─────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::PACK_NAME_COLOR);
    ImGui::PushTextWrapPos(panelW);
    ImGui::TextWrapped("%s", pack->name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // ── Creator / difficulty / shots row ─────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::CREATOR_COLOR);
    if (!pack->creator.empty()) ImGui::Text("By: %s", pack->creator.c_str());
    ImGui::PopStyleColor();

    std::string diff = pack->difficulty.empty() ? "Unranked" : pack->difficulty;
    ImGui::TextColored(DifficultyColor(diff), "%s", diff.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(" · %d shots", pack->shotCount);

    // ── Stats row ─────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::STATS_COLOR);
    ImGui::Text("♥ %d  ▶ %d", pack->likes, pack->plays);
    ImGui::PopStyleColor();

    // ── Tags ──────────────────────────────────────────────────────────────────
    if (!pack->tags.empty()) {
        ImGui::Spacing();
        for (size_t i = 0; i < pack->tags.size(); i++) {
            if (i > 0) ImGui::SameLine(0, 4.0f);
            DrawTagBadge(pack->tags[i].c_str());
        }
    }

    // ── Pack code (small, copyable) ───────────────────────────────────────────
    ImGui::Spacing();
    ImGui::TextDisabled("%s", pack->code.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to copy code");
        if (ImGui::IsItemClicked()) {
            ImGui::SetClipboardText(pack->code.c_str());
            browserStatus.ShowSuccess("Code copied!", 1.5f, UI::StatusMessage::DisplayMode::TimerWithFade);
        }
    }

    // ── Staff comments / notes ────────────────────────────────────────────────
    if (!pack->staffComments.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::COMMENTS_COLOR);
        ImGui::PushTextWrapPos(panelW);
        ImGui::TextWrapped("%s", pack->staffComments.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    // ── Action buttons ────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool isCurrentPostMatch = plugin_->settingsSync && plugin_->settingsSync->GetQuickPicksSelectedCode() == pack->code;

    if (isCurrentPostMatch) {
        ImGui::TextColored(UI::PackBrowserUI::SELECTED_BADGE_COLOR, "Selected for Post-Match");
        ImGui::Spacing();
    }

    float btnH = UI::PackBrowserUI::ACTION_BUTTON_HEIGHT;
    float halfW = (panelW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (!isCurrentPostMatch) {
        if (ImGui::Button("Set Post-Match", ImVec2(halfW, btnH))) {
            plugin_->settingsSync->SetQuickPicksSelected(pack->code);
            plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(pack->code);
            plugin_->settingsSync->SetCurrentTrainingCode(pack->code);
            plugin_->cvarManager->getCvar("suitespot_current_training_code").setValue(pack->code);
            browserStatus.ShowSuccess("Post-Match set: " + pack->name, 2.0f,
                                      UI::StatusMessage::DisplayMode::TimerWithFade);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set this pack to load after matches");
        ImGui::SameLine();
    }

    if (ImGui::Button("Load Now", ImVec2(isCurrentPostMatch ? halfW : halfW, btnH))) {
        LoadPackImmediately(pack->code);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load this pack immediately");

    if (!pack->videoUrl.empty()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, UI::PackBrowserUI::WATCH_BUTTON_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::PackBrowserUI::WATCH_BUTTON_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::PackBrowserUI::WATCH_BUTTON_COLOR);
        // Width = remaining space
        float watchW = panelW - ImGui::GetCursorPosX() + ImGui::GetStyle().WindowPadding.x - 2.0f;
        watchW = std::max(watchW, 60.0f);
        if (ImGui::Button(ICON_FA_PLAY " Watch", ImVec2(watchW, btnH))) {
            ShellExecuteA(NULL, "open", pack->videoUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open video in browser");
        ImGui::PopStyleColor(3);
    }

    // Delete button (custom packs only)
    if (pack->source == "custom") {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, UI::TrainingPackUI::DELETE_BUTTON_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::TrainingPackUI::DELETE_BUTTON_HOVER_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::TrainingPackUI::DELETE_BUTTON_BG_COLOR);
        if (ImGui::Button("Delete Custom Pack", ImVec2(-1.0f, btnH))) {
            if (plugin_->trainingPackMgr) {
                plugin_->trainingPackMgr->DeletePack(pack->code);
                browserStatus.ShowSuccess("Deleted custom pack", 3.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                selectedPackCode.clear();
            }
        }
        ImGui::PopStyleColor(3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RenderCustomPackModal
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::RenderCustomPackModal()
{
    // Center the modal
    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Add Custom Pack", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Add Custom Training Pack");
    ImGui::Separator();
    ImGui::Spacing();

    customPackStatus.Render(ImGui::GetIO().DeltaTime);
    if (customPackStatus.IsVisible()) ImGui::Spacing();

    // Code
    ImGui::TextUnformatted("Code *");
    ImGui::SameLine();
    ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "(XXXX-XXXX-XXXX-XXXX)");
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputText("##mcode", customPackCode, IM_ARRAYSIZE(customPackCode))) {
        // Auto-format: keep only alphanumeric, insert dashes at positions 4,9,14
        std::string raw;
        for (int i = 0; customPackCode[i]; i++) {
            char c = customPackCode[i];
            if (isalnum(static_cast<unsigned char>(c)))
                raw += static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        if (raw.length() > 16) raw = raw.substr(0, 16);
        std::string fmt;
        for (size_t i = 0; i < raw.length(); i++) {
            if (i > 0 && i % 4 == 0) fmt += '-';
            fmt += raw[i];
        }
        strncpy_s(customPackCode, fmt.c_str(), sizeof(customPackCode) - 1);
    }

    // Name
    ImGui::TextUnformatted("Name *");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##mname", customPackName, IM_ARRAYSIZE(customPackName));

    // Creator
    ImGui::TextUnformatted("Creator");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##mcreator", customPackCreator, IM_ARRAYSIZE(customPackCreator));

    // Difficulty + Shot Count on same row
    ImGui::TextUnformatted("Difficulty");
    ImGui::SameLine(120.0f);
    ImGui::TextUnformatted("Shot Count");

    static const char* diffs[] = {"Unranked", "Bronze",         "Silver",           "Gold", "Platinum", "Diamond",
                                  "Champion", "Grand Champion", "Supersonic Legend"};
    ImGui::SetNextItemWidth(150.0f);
    ImGui::Combo("##mdiff", &customPackDifficulty, diffs, IM_ARRAYSIZE(diffs));
    ImGui::SameLine(120.0f);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderInt("##mshots", &customPackShotCount, 1, 50);

    // Tags
    ImGui::TextUnformatted("Tags");
    ImGui::SameLine();
    ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "(comma-separated)");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##mtags", customPackTags, IM_ARRAYSIZE(customPackTags));

    // Notes
    ImGui::TextUnformatted("Notes");
    ImGui::InputTextMultiline("##mnotes", customPackNotes, IM_ARRAYSIZE(customPackNotes), ImVec2(440.0f, 60.0f));

    // Video URL
    ImGui::TextUnformatted("Video URL");
    ImGui::SetNextItemWidth(350.0f);
    ImGui::InputText("##mvideo", customPackVideoUrl, IM_ARRAYSIZE(customPackVideoUrl));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "* Required");
    ImGui::SameLine();

    // Buttons right-aligned
    float addW = ImGui::CalcTextSize("Add Pack").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float cancelW = ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - addW - cancelW - ImGui::GetStyle().ItemSpacing.x);

    if (ImGui::Button("Add Pack", ImVec2(addW, 0))) {
        customPackStatus.Clear();
        if (strlen(customPackCode) == 0)
            customPackStatus.ShowError("Pack code is required");
        else if (!ValidatePackCode(customPackCode))
            customPackStatus.ShowError("Invalid format. Expected: XXXX-XXXX-XXXX-XXXX");
        else if (strlen(customPackName) == 0)
            customPackStatus.ShowError("Pack name is required");
        else {
            TrainingEntry p;
            p.code = customPackCode;
            p.name = customPackName;
            p.creator = strlen(customPackCreator) > 0 ? customPackCreator : "Unknown";
            p.difficulty = diffs[customPackDifficulty];
            p.shotCount = customPackShotCount;
            if (strlen(customPackTags) > 0) {
                std::string tagsStr = customPackTags;
                size_t start = 0, end = tagsStr.find(',');
                while (end != std::string::npos) {
                    auto tag = tagsStr.substr(start, end - start);
                    auto f = tag.find_first_not_of(" \t");
                    auto l = tag.find_last_not_of(" \t");
                    if (f != std::string::npos) p.tags.push_back(tag.substr(f, l - f + 1));
                    start = end + 1;
                    end = tagsStr.find(',', start);
                }
                auto tag = tagsStr.substr(start);
                auto f = tag.find_first_not_of(" \t"), l = tag.find_last_not_of(" \t");
                if (f != std::string::npos) p.tags.push_back(tag.substr(f, l - f + 1));
            }
            p.staffComments = customPackNotes;
            p.videoUrl = customPackVideoUrl;
            p.source = "custom";
            if (plugin_->trainingPackMgr && plugin_->trainingPackMgr->AddCustomPack(p)) {
                browserStatus.ShowSuccess("Pack added: " + p.name, 3.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                ClearCustomPackForm();
                ImGui::CloseCurrentPopup();
            } else {
                customPackStatus.ShowError("Pack with this code already exists");
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(cancelW, 0))) {
        ClearCustomPackForm();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────────────────────
bool TrainingPackUI::ValidatePackCode(const char* code) const
{
    if (strlen(code) != 19) return false;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 9 || i == 14) {
            if (code[i] != '-') return false;
        } else {
            if (!isalnum(static_cast<unsigned char>(code[i]))) return false;
        }
    }
    return true;
}

void TrainingPackUI::ClearCustomPackForm()
{
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

void TrainingPackUI::LoadPackImmediately(const std::string& packCode)
{
    if (packCode.empty() || !plugin_) return;
    if (plugin_->usageTracker) plugin_->usageTracker->IncrementLoadCount(packCode);

    std::string packName = packCode;
    if (const auto* mgr = plugin_->trainingPackMgr.get()) {
        for (const auto& p : mgr->GetPacks()) {
            if (p.code == packCode) {
                packName = p.name;
                break;
            }
        }
    }

    SuiteSpot* plug = plugin_;
    std::string code = packCode;
    plug->gameWrapper->SetTimeout(
        [plug, code, packName](GameWrapper*) {
            plug->cvarManager->executeCommand("load_training " + code);
            LOG("SuiteSpot: Loading training pack: {}", packName);
        },
        0.0f);

    browserStatus.ShowSuccess("Loading: " + packName, 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
    if (plugin_->cvarManager) plugin_->cvarManager->executeCommand("togglemenu settings");
}
