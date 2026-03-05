#include "pch.h"

#include "SettingsUI.h"
#include "LoadoutUI.h"
#include "TrainingPackUI.h"
#include "SuiteSpot.h"
#include "MapManager.h"
#include "TrainingPackManager.h"
#include "WorkshopDownloader.h"
#include "SettingsSync.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"
#include "DefaultPacks.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"

#include "IMGUI/imguivariouscontrols.h"
#include "IMGUI/imgui_searchablecombo.h"
#include "IMGUI/SuiteSpotIcons.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>

SettingsUI::SettingsUI(SuiteSpot* plugin) : plugin_(plugin) {}

// Draws a styled section header: 3px left accent bar + colored label
static void DrawSectionHeader(const char* label, ImU32 accentColor)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 3.0f, p.y + h), accentColor, 1.0f);
    ImGui::Dummy(ImVec2(8.0f, 0.0f));
    ImGui::SameLine(0, 0);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accentColor), "%s", label);
}

void SettingsUI::RenderMainSettingsWindow()
{
    if (!plugin_) {
        return;
    }

    // Style vars (14)
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, UI::ITEM_INNER_SPACING);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, UI::SELECTABLE_TEXT_ALIGN);
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, UI::INDENT_SPACING);
    // Colors (25)
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
    ImGui::PushStyleColor(ImGuiCol_Separator, UI::SEPARATOR_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, UI::SCROLLBAR_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, UI::SCROLLBAR_GRAB_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, UI::SCROLLBAR_GRAB_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, UI::SCROLLBAR_GRAB_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, UI::TEXT_SELECTED_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, UI::POPUP_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, UI::NAV_HIGHLIGHT_COLOR);

    // Lazy-resolve uiFont (queued async in SetImGuiContext; available after first atlas build)
    if (!plugin_->uiFont) {
        auto gui = plugin_->gameWrapper->GetGUIManager();
        plugin_->uiFont = gui.GetFont("suitespot_roboto_14");
    }
    ImGui::PushFont(plugin_->uiFont); // null = default font; replaced by Roboto+FA5 once loaded
    ImGui::SetWindowFontScale(plugin_->uiFont ? 1.0f : UI::FONT_SCALE);

    // Header with metadata and Load Now button
    ImGui::BeginGroup();
    ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "By: Flicks Creations");
    std::string ver = "Version: " + std::string(plugin_version);
    ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "%s", ver.c_str());
    {
        // Format __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss") into "Mmm. dd, yyyy h:mm:ss AM/PM"
        char buildDate[] = __DATE__; // e.g. "Feb 12 2026"
        char buildTime[] = __TIME__; // e.g. "23:50:30"
        int hour = (buildTime[0] - '0') * 10 + (buildTime[1] - '0');
        const char* ampm = hour >= 12 ? "PM" : "AM";
        int hour12 = hour % 12;
        if (hour12 == 0) hour12 = 12;
        // Month is first 3 chars, day starts at index 4, year starts at index 7
        char day[3] = {buildDate[4] == ' ' ? '0' : buildDate[4], buildDate[5], '\0'};
        ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "Built: %.3s. %s, %.4s %d:%.2s:%.2s %s", buildDate, day,
                           buildDate + 7, hour12, buildTime + 3, buildTime + 6, ampm);
    }
    ImGui::EndGroup();

    // Live system clock - drawn directly on the draw list so it doesn't affect layout
    {
        // Retrieve the clock font (queued in onLoad, built asynchronously by BakkesMod)
        if (!plugin_->clockFont) {
            auto gui = plugin_->gameWrapper->GetGUIManager();
            plugin_->clockFont = gui.GetFont("suitespot_clock_48");
        }

        ImFont* font = plugin_->clockFont;
        if (font) {
            std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_s(&local, &now);
            int h = local.tm_hour % 12;
            if (h == 0) h = 12;
            const char* ap = local.tm_hour >= 12 ? "pm" : "am";
            char clockBuf[32];
            snprintf(clockBuf, sizeof(clockBuf), "%d:%02d:%02d%s", h, local.tm_min, local.tm_sec, ap);

            float fontSize = font->FontSize; // Native size — crisp rendering
            ImVec2 clockSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, clockBuf);
            ImVec2 windowPos = ImGui::GetWindowPos();
            float windowWidth = ImGui::GetWindowWidth();
            ImVec2 groupSize = ImGui::GetItemRectSize(); // size of the preceding group
            ImVec2 groupMin = ImGui::GetItemRectMin();   // top-left of the group
            float clockX = windowPos.x + (windowWidth - clockSize.x) * 0.5f;
            float clockY = groupMin.y + (groupSize.y - clockSize.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(font, fontSize, ImVec2(clockX, clockY), IM_COL32(255, 255, 255, 255),
                                                clockBuf);
        }
    }

    {
        float btnW = ImGui::CalcTextSize("LOAD NOW").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - btnW - ImGui::GetStyle().WindowPadding.x);
    }
    if (ImGui::Button("LOAD NOW", ImVec2(0, 26))) {
        int mapType = plugin_->GetMapType();
        SuiteSpot* p = plugin_;
        bool issuedLoad = false;

        if (mapType == 0) { // Freeplay
            std::string code = p->GetCurrentFreeplayCode();
            if (!code.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Freeplay: {})", code);
                p->gameWrapper
                    ->SetTimeout([p, code](GameWrapper* gw) { p->cvarManager->executeCommand("load_freeplay " + code); },
                                 0.0f);
                statusMessage.ShowSuccess("Loading Freeplay", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        } else if (mapType == 1) { // Training
            std::string code = p->settingsSync->GetQuickPicksSelectedCode();
            if (code.empty()) code = p->GetCurrentTrainingCode();

            if (!code.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Training: {})", code);
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(code);
                p->gameWrapper
                    ->SetTimeout([p, code](GameWrapper* gw) { p->cvarManager->executeCommand("load_training " + code); },
                                 0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        } else if (mapType == 2) { // Workshop
            std::string path = p->GetCurrentWorkshopPath();
            if (!path.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Workshop: {})", path);
                p->gameWrapper->SetTimeout(
                    [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                    0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        }

        if (issuedLoad && p->cvarManager) {
            p->cvarManager->executeCommand("togglemenu settings");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Immediately load the currently selected map/pack");
    }

    ImGui::Spacing();
    statusMessage.Render(ImGui::GetIO().DeltaTime);
    if (statusMessage.IsVisible()) ImGui::Spacing();

    bool enabledValue = plugin_->IsEnabled();
    int mapTypeValue = plugin_->GetMapType();
    bool autoQueueValue = plugin_->IsAutoQueueEnabled();
    int delayQueueSecValue = plugin_->GetDelayQueueSec();
    int delayFreeplaySecValue = plugin_->GetDelayFreeplaySec();
    int delayTrainingSecValue = plugin_->GetDelayTrainingSec();
    int delayWorkshopSecValue = plugin_->GetDelayWorkshopSec();
    std::string currentFreeplayCode = plugin_->GetCurrentFreeplayCode();
    std::string currentTrainingCode = plugin_->GetCurrentTrainingCode();
    std::string quickPicksSelectedCode = plugin_->settingsSync->GetQuickPicksSelectedCode();
    std::string currentWorkshopPath = plugin_->GetCurrentWorkshopPath();

    // Only show status if enabled
    if (enabledValue) {
        ImGui::Separator();

        const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
        std::string currentMap = "<none>";

        // Get current selection
        if (mapTypeValue == 0) {
            auto it = std::find_if(RLMaps.begin(), RLMaps.end(),
                                   [&](const MapEntry& e) { return e.code == currentFreeplayCode; });
            if (it != RLMaps.end()) currentMap = it->name;
        } else if (mapTypeValue == 1) {
            std::string targetCode = quickPicksSelectedCode;
            if (targetCode.empty()) targetCode = currentTrainingCode;
            auto targetPack = plugin_->trainingPackMgr->GetPackByCode(targetCode);
            if (targetPack) {
                currentMap = targetPack->name;
            } else if (!targetCode.empty()) {
                currentMap = targetCode + " (custom)";
            }
        } else if (mapTypeValue == 2) {
            auto it = std::find_if(RLWorkshop.begin(), RLWorkshop.end(),
                                   [&](const WorkshopEntry& e) { return e.filePath == currentWorkshopPath; });
            if (it != RLWorkshop.end()) currentMap = it->name;
        }

        const ImVec4 green = UI::SettingsUI::STATUS_ENABLED_TEXT_COLOR;
        const ImVec4 red = UI::SettingsUI::STATUS_DISABLED_TEXT_COLOR;
        const ImVec4 orange = UI::STATUS_WARN_COLOR;
        const ImVec4 accent = UI::SECTION_HEADER_COLOR;
        const ImVec4 dim = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        // Status bar as framed child panel (single line)
        float sbH = ImGui::GetTextLineHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + 2.0f;
        ImGui::BeginChildFrame(ImGui::GetID("##StatusBar"), ImVec2(-1.0f, sbH));
        {
            ImGui::TextColored(accent, ICON_FA_CIRCLE);
            ImGui::SameLine(0, 5);
            ImGui::TextColored(green, "Mode: %s", modeNames[mapTypeValue]);

            ImGui::SameLine(0, 10);
            ImGui::TextColored(dim, "|");
            ImGui::SameLine(0, 10);

            ImGui::Text("Map:");
            ImGui::SameLine(0, 4);
            bool noMap = (currentMap == "<none>" || currentMap == "<none selected>");
            ImGui::TextColored(noMap ? orange : green, "%s", currentMap.c_str());

            ImGui::SameLine(0, 10);
            ImGui::TextColored(dim, "|");
            ImGui::SameLine(0, 10);

            const ImVec4 queueColor = autoQueueValue ? green : red;
            ImGui::TextColored(queueColor, "Auto-Queue: %s", autoQueueValue ? "ON" : "OFF");
            if (autoQueueValue && delayQueueSecValue > 0) {
                ImGui::SameLine(0, 4);
                ImGui::TextDisabled("(+%ds queue)", delayQueueSecValue);
            }

            int currentDelay = (mapTypeValue == 0) ? delayFreeplaySecValue
                                                   : (mapTypeValue == 1 ? delayTrainingSecValue : delayWorkshopSecValue);
            if (currentDelay > 0) {
                ImGui::SameLine(0, 10);
                ImGui::TextColored(dim, "|");
                ImGui::SameLine(0, 10);
                ImGui::TextDisabled("Map Delay: +%ds", currentDelay);
            }
        }
        ImGui::EndChildFrame();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1) Global Controls
    // ...
    ImGui::Spacing();

    // 1) Global Controls (Enable/Disable + Map Mode) - above the tabs
    RenderGeneralTab(enabledValue, mapTypeValue);
    ImGui::Spacing();
    ImGui::Separator();

    // Main tab bar - disabled when plugin is off
    if (!enabledValue) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::BeginTabBar("SuiteSpotTabs", ImGuiTabBarFlags_None)) {

        // ===== MAP SELECT TAB =====
        if (ImGui::BeginTabItem(ICON_FA_MAP " Map Select")) {
            if (enabledValue) {
                ImGui::Spacing();

                // Compact single row: [Auto-Queue]  Queue Delay: [__] s  Map Delay: [__] s
                UI::Helpers::CheckboxWithCVar("Auto-Queue", autoQueueValue, "suitespot_auto_queue",
                                              plugin_->cvarManager, plugin_->gameWrapper,
                                              "Automatically queue into the next match after the current match ends.");
                ImGui::SameLine(0, 20);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Queue Delay:");
                ImGui::SameLine(0, 6);
                ImGui::PushButtonRepeat(true);
                UI::Helpers::InputIntWithRange("##QueueDelay", delayQueueSecValue,
                                               UI::SettingsUI::DELAY_QUEUE_MIN_SECONDS,
                                               UI::SettingsUI::DELAY_QUEUE_MAX_SECONDS, 75.0f,
                                               "suitespot_delay_queue_sec", plugin_->cvarManager, plugin_->gameWrapper,
                                               "Wait before auto-queuing.", nullptr);
                ImGui::PopButtonRepeat();
                ImGui::SameLine(0, 3);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("s");
                ImGui::SameLine(0, 16);

                // Map Delay (context-sensitive CVar based on current map type)
                int* currentMapDelayValue = &delayFreeplaySecValue;
                const char* currentMapDelayCVar = "suitespot_delay_freeplay_sec";
                const char* mapDelayTooltip = "Wait before loading Freeplay.";
                if (mapTypeValue == 1) {
                    currentMapDelayValue = &delayTrainingSecValue;
                    currentMapDelayCVar = "suitespot_delay_training_sec";
                    mapDelayTooltip = "Wait before loading Training.";
                } else if (mapTypeValue == 2) {
                    currentMapDelayValue = &delayWorkshopSecValue;
                    currentMapDelayCVar = "suitespot_delay_workshop_sec";
                    mapDelayTooltip = "Wait before loading Workshop.";
                }

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Map Delay:");
                ImGui::SameLine(0, 6);
                ImGui::PushButtonRepeat(true);
                UI::Helpers::InputIntWithRange("##MapDelay", *currentMapDelayValue, 0, 300, 75.0f, currentMapDelayCVar,
                                               plugin_->cvarManager, plugin_->gameWrapper, mapDelayTooltip, nullptr);
                ImGui::PopButtonRepeat();
                ImGui::SameLine(0, 3);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("s");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 2) Map Selection Logic
                RenderMapSelectionTab(mapTypeValue, currentFreeplayCode, currentTrainingCode, currentWorkshopPath,
                                      delayFreeplaySecValue, delayTrainingSecValue, delayWorkshopSecValue,
                                      delayQueueSecValue);
            } else {
                ImGui::Spacing();
                ImGui::TextDisabled("Enable SuiteSpot to configure map settings.");
            }

            ImGui::EndTabItem();
        }

        // ===== LOADOUT MANAGEMENT TAB =====
        if (ImGui::BeginTabItem(ICON_FA_LIST " Loadout")) {
            if (enabledValue) {
                if (plugin_->loadoutUI) {
                    plugin_->loadoutUI->RenderLoadoutControls();
                }
            } else {
                ImGui::Spacing();
                ImGui::TextDisabled("Enable SuiteSpot to manage loadouts.");
            }
            ImGui::EndTabItem();
        }

        // ===== HOTKEYS TAB =====
        if (ImGui::BeginTabItem(ICON_FA_KEYBOARD " Hotkeys")) {
            ImGui::Spacing();
            DrawSectionHeader("Keyboard Shortcuts", ImGui::ColorConvertFloat4ToU32(UI::SECTION_HEADER_COLOR));
            ImGui::Spacing();
            ImGui::TextDisabled("Click " ICON_FA_CIRCLE " to capture a key press, or type the UE3 name manually.");
            ImGui::TextDisabled("Key 1 = trigger.  Key 2 = required held modifier.  Both must be set.");
            ImGui::TextDisabled("Xbox buttons are captured automatically.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Columns(2, "HotkeysTabCols", false);
            ImGui::SetColumnWidth(0, UI::SettingsUI::HOTKEY_LABEL_COL_WIDTH);

            for (int i = 0; i < 5; i++) {
                const auto& row = UI::SettingsUI::HOTKEY_ROWS[i];
                ImGui::PushID(i);

                // Col 0: action label
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(row.label);
                ImGui::NextColumn();

                // Helper: render one key slot (key1 or key2)
                auto renderSlot = [&](int slot, const char* cvarName, const char* inputId, const char* capBtnId,
                                      const char* clrBtnId, const char* tip) {
                    char buf[64] = {};
                    if (auto cvar = plugin_->cvarManager->getCvar(cvarName); !cvar.IsNull())
                        strncpy_s(buf, cvar.getStringValue().c_str(), sizeof(buf) - 1);

                    bool cap = (plugin_->captureRow == i && plugin_->captureSlot == slot);
                    if (cap) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                        ImGui::Button("Press any key...", ImVec2(UI::SettingsUI::HOTKEY_KEY_INPUT_WIDTH, 0));
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::SetNextItemWidth(UI::SettingsUI::HOTKEY_KEY_INPUT_WIDTH);
                        if (ImGui::InputText(inputId, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
                            UI::Helpers::SetCVarSafely(cvarName, std::string(buf), plugin_->cvarManager,
                                                       plugin_->gameWrapper);
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            UI::Helpers::SetCVarSafely(cvarName, std::string(buf), plugin_->cvarManager,
                                                       plugin_->gameWrapper);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
                    }
                    ImGui::SameLine(0, 2);
                    ImGui::PushID(slot);                                           // Unique ID per slot within the row
                    if (ImGui::SmallButton(cap ? ICON_FA_STOP : ICON_FA_CIRCLE)) { // stop / record
                        plugin_->captureRow = cap ? -1 : i;
                        plugin_->captureSlot = slot;
                    }
                    ImGui::PopID();
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetTooltip(cap ? "Cancel (or press Esc)"
                                              : "Click then press any key (Keyboard or Xbox) to capture");
                    }
                    if (!cap && buf[0] != '\0') {
                        ImGui::SameLine(0, 2);
                        if (ImGui::SmallButton(clrBtnId))
                            UI::Helpers::SetCVarSafely(cvarName, std::string(""), plugin_->cvarManager,
                                                       plugin_->gameWrapper);
                    }
                };

                // Col 1: [Key1 slot]  +  [Key2 slot]
                renderSlot(0, row.key1CVar, "##k1", "##cb1", "X##k1x",
                           "Trigger key - click " ICON_FA_CIRCLE " to capture, or type UE3 name");
                ImGui::SameLine(0, 6);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("+");
                ImGui::SameLine(0, 6);
                renderSlot(1, row.key2CVar, "##k2", "##cb2", "X##k2x",
                           "Required held modifier — must be held when Key 1 fires. Both keys must be set.");

                // Validation: key1 set without key2 is invalid (dual-key required)
                {
                    char k1[64] = {}, k2[64] = {};
                    if (auto c = plugin_->cvarManager->getCvar(row.key1CVar); !c.IsNull())
                        strncpy_s(k1, c.getStringValue().c_str(), sizeof(k1) - 1);
                    if (auto c = plugin_->cvarManager->getCvar(row.key2CVar); !c.IsNull())
                        strncpy_s(k2, c.getStringValue().c_str(), sizeof(k2) - 1);
                    if (k1[0] != '\0' && k2[0] == '\0') {
                        ImGui::SameLine(0, 8);
                        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "(!) set Key 2");
                    }
                }

                ImGui::NextColumn();
                ImGui::PopID();
                ImGui::Spacing();
            }
            ImGui::Columns(1);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_SAVE " Save Hotkeys")) plugin_->cvarManager->executeCommand("writeconfig", false);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Persist hotkey settings to config file");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!enabledValue) {
        ImGui::PopStyleVar();
    }

    ImGui::PopFont();
    ImGui::PopStyleColor(25); // all color pushes
    ImGui::PopStyleVar(14);   // all style var pushes
}

void SettingsUI::RenderGeneralTab(bool& enabledValue, int& mapTypeValue)
{
    // Row: [Enable SuiteSpot]  [Fix Training Speed]  ···  Mode: [Freeplay][Training][Workshop]
    UI::Helpers::CheckboxWithCVar("Enable SuiteSpot", enabledValue, "suitespot_enabled", plugin_->cvarManager,
                                  plugin_->gameWrapper, "Enable/disable all SuiteSpot auto-loading and queuing features");
    ImGui::SameLine(0, 20);

    bool gameSpeedFixEnabled = plugin_->settingsSync ? plugin_->settingsSync->IsTrainingGameSpeedFixEnabled() : true;
    if (UI::Helpers::CheckboxWithCVar("Fix Training Speed", gameSpeedFixEnabled, "suitespot_fix_training_gamespeed",
                                      plugin_->cvarManager, plugin_->gameWrapper,
                                      "Sync in-game training speed with sv_soccar_gamespeed in training playlists.")) {
        LOG("Fix Training Game Speed toggled to {}", gameSpeedFixEnabled ? "ON" : "OFF");
    }

    // Right-align map mode CheckButtons
    const char* modeLabels[] = {ICON_FA_GLOBE " Freeplay", ICON_FA_GRADUATION_CAP " Training", ICON_FA_COGS " Workshop"};
    float modeGroupW = ImGui::CalcTextSize("Mode:").x + ImGui::GetStyle().ItemSpacing.x;
    for (int i = 0; i < 3; i++) {
        modeGroupW += ImGui::CalcTextSize(modeLabels[i]).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (i < 2) modeGroupW += 4.0f; // SameLine spacing between buttons
    }
    ImGui::SameLine(ImGui::GetContentRegionMax().x - modeGroupW - ImGui::GetStyle().WindowPadding.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine(0, 4);
        bool active = (mapTypeValue == i);
        bool wasActive = active; // snapshot BEFORE CheckButton modifies it
        if (wasActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, UI::MAP_MODE_ACTIVE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::MAP_MODE_ACTIVE_COLOR);
        }
        if (ImGui::CheckButton(modeLabels[i], &active) && active) {
            mapTypeValue = i;
            LOG("SuiteSpot UI: User switched Map Mode to {}", modeLabels[i]);
            UI::Helpers::SetCVarSafely("suitespot_map_type", mapTypeValue, plugin_->cvarManager, plugin_->gameWrapper);
        }
        if (wasActive) ImGui::PopStyleColor(2); // pop must match push count
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Choose which map type loads after matches:\nFreeplay = Official | Training = Custom Packs | "
                          "Workshop = Modded Maps");
    }
}

void SettingsUI::RenderMapSelectionTab(int mapTypeValue, std::string& currentFreeplayCode,
                                       std::string& currentTrainingCode, std::string& currentWorkshopPath,
                                       int& delayFreeplaySecValue, int& delayTrainingSecValue,
                                       int& delayWorkshopSecValue, int& delayQueueSecValue)
{
    DrawSectionHeader("Map Selection", ImGui::ColorConvertFloat4ToU32(UI::SECTION_HEADER_COLOR));
    ImGui::Spacing();

    if (mapTypeValue == 0) {
        RenderFreeplayMode(currentFreeplayCode);
    } else if (mapTypeValue == 1) {
        RenderTrainingMode(0, currentTrainingCode);
    } else if (mapTypeValue == 2) {
        RenderWorkshopMode(currentWorkshopPath);
    }
}

void SettingsUI::RenderFreeplayMode(std::string& currentFreeplayCode)
{
    // Initialize to first map if empty and maps are available
    if (currentFreeplayCode.empty() && !RLMaps.empty()) {
        currentFreeplayCode = RLMaps[0].code;
        plugin_->settingsSync->SetCurrentFreeplayCode(currentFreeplayCode);
        plugin_->cvarManager->getCvar("suitespot_current_freeplay_code").setValue(currentFreeplayCode);
    }

    // Find current selection index
    int currentIndex = 0;
    for (int i = 0; i < (int)RLMaps.size(); i++) {
        if (RLMaps[i].code == currentFreeplayCode) {
            currentIndex = i;
            break;
        }
    }

    const char* freeplayLabel = RLMaps.empty() ? "<none>" : RLMaps[currentIndex].name.c_str();

    // Build names list for SearchableCombo
    std::vector<std::string> mapNames;
    mapNames.reserve(RLMaps.size());
    for (const auto& m : RLMaps)
        mapNames.push_back(m.name);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Freeplay Map:");
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(UI::SettingsUI::FREEPLAY_MAPS_DROPDOWN_WIDTH);
    if (ImGui::SearchableCombo("##FreeplayMap", &currentIndex, mapNames, freeplayLabel, "Search maps...")) {
        if (currentIndex >= 0 && currentIndex < (int)RLMaps.size()) {
            currentFreeplayCode = RLMaps[currentIndex].code;
            LOG("SuiteSpot UI: User selected Freeplay map: {} ({})", RLMaps[currentIndex].name, currentFreeplayCode);
            plugin_->settingsSync->SetCurrentFreeplayCode(currentFreeplayCode);
            plugin_->cvarManager->getCvar("suitespot_current_freeplay_code").setValue(currentFreeplayCode);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select which stadium loads after matches");
}

void SettingsUI::RenderTrainingMode(int trainingModeValue, std::string& currentTrainingCode)
{
    RenderSinglePackMode(currentTrainingCode);
}

void SettingsUI::RenderWorkshopMode(std::string& currentWorkshopPath)
{
    if (ImGui::BeginTabBar("##WorkshopSubTabs")) {
        if (ImGui::BeginTabItem("Installed Maps")) {
            RenderInstalledMaps(currentWorkshopPath);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Map Browser")) {
            RenderWorkshopBrowserTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void SettingsUI::RenderInstalledMaps(std::string& currentWorkshopPath)
{
    // Header with Refresh button (right-aligned)
    ImGui::Spacing();
    {
        float btnW = ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - btnW);
    }
    if (ImGui::Button("Refresh", ImVec2(0, 0))) {
        plugin_->LoadWorkshopMaps();
        selectedWorkshopIndex = -1; // Reset selection
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rescan workshop folders for maps");
    }
    ImGui::Spacing();

    // Check if we have any maps
    if (RLWorkshop.empty()) {
        ImGui::TextColored(UI::WorkshopBrowserUI::NO_MAPS_COLOR, "No workshop maps found.");
        ImGui::TextDisabled("Maps are discovered from:");
        ImGui::BulletText("WorkshopMapLoader configured path");
        ImGui::BulletText("Epic Games install mods folder");
        ImGui::BulletText("Steam install mods folder");
        ImGui::Spacing();
        ImGui::TextDisabled("Download maps from the Map Browser tab.");
        return;
    }

    // Initialize selection from current CVar if needed
    if (selectedWorkshopIndex < 0 && !currentWorkshopPath.empty()) {
        for (int i = 0; i < (int)RLWorkshop.size(); i++) {
            if (RLWorkshop[i].filePath == currentWorkshopPath) {
                selectedWorkshopIndex = i;
                break;
            }
        }
    }

    // Clamp selection to valid range
    if (selectedWorkshopIndex >= (int)RLWorkshop.size()) {
        selectedWorkshopIndex = (int)RLWorkshop.size() - 1;
    }

    // Calculate panel widths
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::WorkshopBrowserUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::WorkshopBrowserUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    // Two-panel layout
    ImGui::BeginGroup();

    // === LEFT PANEL: Map List ===
    if (ImGui::BeginChild("WorkshopMapList", ImVec2(leftWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        // Filter input
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##WorkshopInstFilter", "Filter maps...", workshopInstalledFilterBuf,
                                 sizeof(workshopInstalledFilterBuf));

        // Build filter string (lowercase for case-insensitive match)
        std::string filterStr = workshopInstalledFilterBuf;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

        // Count visible maps for header
        int visibleCount = 0;
        for (const auto& e : RLWorkshop) {
            if (filterStr.empty()) {
                visibleCount++;
                continue;
            }
            std::string nl = e.name;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            if (nl.find(filterStr) != std::string::npos) visibleCount++;
        }
        ImGui::TextDisabled(filterStr.empty() ? "%d maps" : "%d / %d maps", visibleCount, (int)RLWorkshop.size());
        ImGui::Separator();

        for (int i = 0; i < (int)RLWorkshop.size(); i++) {
            const auto& entry = RLWorkshop[i];

            // Apply filter
            if (!filterStr.empty()) {
                std::string nameLower = entry.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(filterStr) == std::string::npos) continue;
            }

            bool isSelected = (i == selectedWorkshopIndex);
            bool isCurrentAutoLoad = (entry.filePath == currentWorkshopPath);

            ImGui::PushID(i);

            if (isCurrentAutoLoad) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::SELECTED_BADGE_COLOR);
                ImGui::Text(">");
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            if (ImGui::Selectable(entry.name.c_str(), isSelected, ImGuiSelectableFlags_None)) {
                selectedWorkshopIndex = i;
                currentWorkshopPath = entry.filePath;
                LOG("SuiteSpot UI: User selected Workshop map: {} ({})", entry.name, entry.filePath);
                plugin_->settingsSync->SetCurrentWorkshopPath(entry.filePath);
                if (auto cvar = plugin_->cvarManager->getCvar("suitespot_current_workshop_path")) {
                    cvar.setValue(entry.filePath);
                }
            }

            // Double-click to load immediately
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                SuiteSpot* p = plugin_;
                std::string path = entry.filePath;
                p->gameWrapper->SetTimeout(
                    [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                    0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
            }

            // Hand cursor on hover
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##WsCtx")) {
                if (ImGui::MenuItem("Load Now")) {
                    SuiteSpot* p = plugin_;
                    std::string path = entry.filePath;
                    p->gameWrapper->SetTimeout(
                        [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                        0.0f);
                    statusMessage.ShowSuccess("Loading Workshop Map", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                    if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
                }
                if (!isCurrentAutoLoad && ImGui::MenuItem("Select for Post-Match")) {
                    selectedWorkshopIndex = i;
                    currentWorkshopPath = entry.filePath;
                    plugin_->settingsSync->SetCurrentWorkshopPath(entry.filePath);
                    if (auto cvar = plugin_->cvarManager->getCvar("suitespot_current_workshop_path"))
                        cvar.setValue(entry.filePath);
                    statusMessage.ShowSuccess("Workshop map selected", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                ImGui::EndPopup();
            }

            // Scroll to selected on first appearance
            if (isSelected && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT PANEL: Details Pane ===
    if (ImGui::BeginChild("WorkshopMapDetails", ImVec2(rightWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        if (selectedWorkshopIndex >= 0 && selectedWorkshopIndex < (int)RLWorkshop.size()) {
            auto& selectedMap = RLWorkshop[selectedWorkshopIndex];

            // Load preview image if needed (lazy loading)
            if (!selectedMap.previewPath.empty() && !selectedMap.isImageLoaded && !selectedMap.previewImage) {
                selectedMap.previewImage = std::make_shared<ImageWrapper>(selectedMap.previewPath.string(), false, true);
                selectedMap.isImageLoaded = true;
            }

            // Preview image
            if (selectedMap.previewImage && selectedMap.previewImage->GetImGuiTex()) {
                ImGui::Image(selectedMap.previewImage->GetImGuiTex(),
                             ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                    UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            } else {
                // Placeholder box
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(p,
                                        ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                               p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT),
                                        ImColor(40, 40, 45, 255), 4.0f);
                drawList->AddText(ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH / 2 - 40,
                                         p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT / 2 - 8),
                                  ImColor(100, 100, 100, 255), "No Preview");
                ImGui::Dummy(
                    ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH, UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            }

            ImGui::Spacing();

            // Map name (large)
            ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::MAP_NAME_COLOR);
            ImGui::TextWrapped("%s", selectedMap.name.c_str());
            ImGui::PopStyleColor();

            // Author
            if (!selectedMap.author.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::AUTHOR_COLOR);
                ImGui::Text("By: %s", selectedMap.author.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            // Description
            if (!selectedMap.description.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::DESCRIPTION_COLOR);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", selectedMap.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Current selection indicator
            bool isCurrentAutoLoad = (selectedMap.filePath == currentWorkshopPath);
            if (isCurrentAutoLoad) {
                ImGui::TextColored(UI::WorkshopBrowserUI::SELECTED_BADGE_COLOR, "Selected for Post-Match Auto-Load");
                ImGui::Spacing();
            }

            // Action buttons
            if (!isCurrentAutoLoad) {
                if (ImGui::Button("Select for Post-Match", ImVec2(0, 26))) {
                    plugin_->settingsSync->SetCurrentWorkshopPath(selectedMap.filePath);
                    plugin_->cvarManager->getCvar("suitespot_current_workshop_path").setValue(selectedMap.filePath);
                    currentWorkshopPath = selectedMap.filePath;
                    statusMessage.ShowSuccess("Workshop map selected", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Set this map to load after matches end");
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Load Now", ImVec2(0, 26))) {
                SuiteSpot* p = plugin_;
                std::string path = selectedMap.filePath;
                p->gameWrapper->SetTimeout(
                    [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                    0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) {
                    p->cvarManager->executeCommand("togglemenu settings");
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Load this workshop map immediately");
            }

        } else {
            // No selection
            ImGui::TextDisabled("Select a map from the list");
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

void SettingsUI::RenderSinglePackMode(std::string& currentTrainingCode)
{
    // Toggle between Flicks Picks and Your Favorites
    int listType = plugin_->settingsSync->GetQuickPicksListType();

    ImGui::TextUnformatted("List Type:");
    ImGui::SameLine();

    if (ImGui::RadioButton("Flicks Picks", listType == 0)) {
        UI::Helpers::SetCVarSafely("suitespot_quickpicks_list_type", 0, plugin_->cvarManager, plugin_->gameWrapper);
        selectedQuickPickIndex = -1;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Curated selection of 10 essential training packs");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Quick Picks", listType == 1)) {
        UI::Helpers::SetCVarSafely("suitespot_quickpicks_list_type", 1, plugin_->cvarManager, plugin_->gameWrapper);
        selectedQuickPickIndex = -1;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Your most-used training packs based on load history");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_SEARCH " Open Pack Browser", ImVec2(0, 0))) {
        SuiteSpot* p = plugin_;
        p->gameWrapper
            ->SetTimeout([p](GameWrapper* gw) { p->cvarManager->executeCommand("togglemenu suitespot_browser"); }, 0.0f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open the full training pack browser to manage bags and packs");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Build pack metadata list for this render pass
    std::vector<std::string> quickPicks = GetQuickPicksList();
    std::string selectedCode = plugin_->settingsSync->GetQuickPicksSelectedCode();

    // Default selection on first render
    if (selectedCode.empty() && !quickPicks.empty()) {
        selectedCode = quickPicks[0];
        plugin_->settingsSync->SetQuickPicksSelected(selectedCode);
        plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(selectedCode);
    }

    // Clamp index
    if (selectedQuickPickIndex >= (int)quickPicks.size()) {
        selectedQuickPickIndex = -1;
    }

    // Sync selectedQuickPickIndex from selectedCode on first render (fixes empty right panel)
    if (selectedQuickPickIndex < 0 && !selectedCode.empty()) {
        for (int i = 0; i < (int)quickPicks.size(); i++) {
            if (quickPicks[i] == selectedCode) {
                selectedQuickPickIndex = i;
                break;
            }
        }
    }

    // Helper: resolve TrainingEntry* by code
    auto resolveEntry = [&](const std::string& code) -> const TrainingEntry* {
        const auto& packs = plugin_->trainingPackMgr ? plugin_->trainingPackMgr->GetPacks() : RLTraining;
        auto it = std::find_if(packs.begin(), packs.end(), [&](const TrainingEntry& e) { return e.code == code; });
        if (it != packs.end()) return &(*it);
        return nullptr;
    };

    // Calculate panel widths
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::QuickPicksUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::QuickPicksUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup();

    // === LEFT PANEL: Pack List ===
    if (ImGui::BeginChild("QuickPicksList", ImVec2(leftWidth, UI::QuickPicksUI::BROWSER_HEIGHT), true)) {
        const char* header = (listType == 0) ? "Flicks Picks" : "Quick Picks";
        ImGui::TextDisabled("%s  (%d packs)", header, (int)quickPicks.size());
        ImGui::Separator();

        for (int i = 0; i < (int)quickPicks.size(); i++) {
            const std::string& code = quickPicks[i];
            bool isPostMatch = (code == selectedCode);
            bool isHighlighted = (i == selectedQuickPickIndex);

            ImGui::PushID(i);

            // Post-match selection marker
            if (isPostMatch) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::SELECTED_BADGE_COLOR);
                ImGui::TextUnformatted(">");
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            // Resolve display name
            std::string displayName = code;
            if (const TrainingEntry* e = resolveEntry(code)) {
                displayName = e->name;
            } else {
                for (const auto& d : DefaultPacks::FLICKS_PICKS) {
                    if (d.code == code) {
                        displayName = d.name;
                        break;
                    }
                }
            }

            if (ImGui::Selectable(displayName.c_str(), isHighlighted)) {
                selectedQuickPickIndex = i;
            }

            // Double-click to load immediately
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                SuiteSpot* p = plugin_;
                std::string c = code;
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(c);
                p->gameWrapper->SetTimeout([p, c](GameWrapper*) { p->cvarManager->executeCommand("load_training " + c); },
                                           0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
            }

            // Hand cursor on hover
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##QpCtx")) {
                if (ImGui::MenuItem("Load Now")) {
                    SuiteSpot* p = plugin_;
                    std::string c = code;
                    if (p->usageTracker) p->usageTracker->IncrementLoadCount(c);
                    p->gameWrapper
                        ->SetTimeout([p, c](GameWrapper*) { p->cvarManager->executeCommand("load_training " + c); },
                                     0.0f);
                    statusMessage.ShowSuccess("Loading Training Pack", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                    if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
                }
                if (!isPostMatch && ImGui::MenuItem("Select for Post-Match")) {
                    selectedCode = code;
                    plugin_->settingsSync->SetQuickPicksSelected(code);
                    plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(code);
                    statusMessage.ShowSuccess("Pack selected for post-match", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::MenuItem("Copy Code")) {
                    ImGui::SetClipboardText(code.c_str());
                    statusMessage.ShowSuccess("Code copied!", 1.5f, UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                ImGui::EndPopup();
            }

            // Scroll to selected item on first appear
            if (isHighlighted && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT PANEL: Details ===
    if (ImGui::BeginChild("QuickPicksDetails", ImVec2(rightWidth, UI::QuickPicksUI::BROWSER_HEIGHT), true)) {
        if (selectedQuickPickIndex >= 0 && selectedQuickPickIndex < (int)quickPicks.size()) {
            const std::string& code = quickPicks[selectedQuickPickIndex];
            bool isPostMatch = (code == selectedCode);

            // Resolve metadata — try live cache first, then DefaultPacks
            std::string name = code;
            std::string creator;
            std::string difficulty;
            std::string description;
            std::vector<std::string> tags;
            int shots = 0;

            if (const TrainingEntry* e = resolveEntry(code)) {
                name = e->name;
                creator = e->creator;
                difficulty = e->difficulty;
                shots = e->shotCount;
                description = e->staffComments.empty() ? e->notes : e->staffComments;
                tags = e->tags;
            } else {
                for (const auto& d : DefaultPacks::FLICKS_PICKS) {
                    if (d.code == code) {
                        name = d.name;
                        shots = d.shotCount;
                        description = d.description;
                        break;
                    }
                }
            }

            // Pack name
            ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::PACK_NAME_COLOR);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopStyleColor();

            // Pack code (copyable hint)
            ImGui::TextDisabled("Code: %s", code.c_str());

            // Creator
            if (!creator.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::CREATOR_COLOR);
                ImGui::Text("By: %s", creator.c_str());
                ImGui::PopStyleColor();
            }

            // Shots + difficulty on same line
            if (shots > 0) ImGui::Text("%d shots", shots);
            if (!difficulty.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("| %s", difficulty.c_str());
            }

            // Tags
            if (!tags.empty()) {
                ImGui::Spacing();
                for (const auto& tag : tags) {
                    ImGui::TextDisabled("[%s]", tag.c_str());
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

            // Description
            if (!description.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::DESCRIPTION_COLOR);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Post-match selection indicator
            if (isPostMatch) {
                ImGui::TextColored(UI::QuickPicksUI::SELECTED_BADGE_COLOR, "Selected for Post-Match Auto-Load");
                ImGui::Spacing();
            }

            // Action buttons
            if (!isPostMatch) {
                if (ImGui::Button("Select for Post-Match", ImVec2(0, 26))) {
                    selectedCode = code;
                    plugin_->settingsSync->SetQuickPicksSelected(code);
                    plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(code);
                    statusMessage.ShowSuccess("Pack selected for post-match", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Load this pack automatically after matches end");
                }
                ImGui::SameLine();
            }

            if (ImGui::Button("Load Now", ImVec2(0, 26))) {
                SuiteSpot* p = plugin_;
                std::string c = code;
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(c);
                p->gameWrapper->SetTimeout([p, c](GameWrapper*) { p->cvarManager->executeCommand("load_training " + c); },
                                           0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Load this training pack immediately");
            }

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_COPY " Copy Code", ImVec2(0, 26))) {
                ImGui::SetClipboardText(code.c_str());
                statusMessage.ShowSuccess("Code copied!", 1.5f, UI::StatusMessage::DisplayMode::TimerWithFade);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy pack code: %s", code.c_str());

        } else {
            ImGui::TextDisabled("Select a pack from the list");
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

std::vector<std::string> SettingsUI::GetQuickPicksList()
{
    int listType = plugin_->settingsSync->GetQuickPicksListType();

    // Build Flicks Picks list
    std::vector<std::string> flicksPicks;
    for (const auto& p : DefaultPacks::FLICKS_PICKS) {
        flicksPicks.push_back(p.code);
    }

    // If Flicks Picks mode is selected, always return Flicks Picks
    if (listType == 0) {
        return flicksPicks;
    }

    // Your Quick Picks mode - use usage tracker
    if (!plugin_->usageTracker) return flicksPicks;

    // If first run or no data, fallback to Flicks Picks
    if (plugin_->usageTracker->IsFirstRun()) {
        return flicksPicks;
    }

    int count = plugin_->settingsSync->GetQuickPicksCount();
    auto topCodes = plugin_->usageTracker->GetTopUsedCodes(count);

    // If no favorites yet, fallback to Flicks Picks
    if (topCodes.empty()) return flicksPicks;

    return topCodes;
}

// ── Workshop Search Scoring ────────────────────────────────────────────────
// Returns relevance score >= 1 if query found in result, -1 if no match.
// Higher = better match. Uses search-engine best practices:
//   1000 = exact name match
//    900 = name starts with query (prefix)
//    800 = any word in name starts with query (word boundary)
//    700-N = query found at position N in name (earlier = better)
//    +50 per extra occurrence (frequency bonus)
//    +100 if author also matches
static int ScoreResult(const std::string& queryLower, const RLMAPS_MapResult& result)
{
    if (queryLower.empty()) return 1; // No filter: everything passes

    std::string nameLower = result.Name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    size_t firstPos = nameLower.find(queryLower);
    if (firstPos == std::string::npos) return -1; // No match → exclude

    int score = 0;

    // Tier 1: exact match
    if (nameLower == queryLower) {
        score = 1000;
    }
    // Tier 2: prefix match (name starts with query)
    else if (nameLower.compare(0, queryLower.size(), queryLower) == 0) {
        score = 900;
    }
    // Tier 3: word boundary match
    else {
        bool wordBoundary = false;
        // A word boundary = position 0 or preceded by space
        size_t searchPos = 0;
        while ((searchPos = nameLower.find(queryLower, searchPos)) != std::string::npos) {
            if (searchPos == 0 || nameLower[searchPos - 1] == ' ') {
                wordBoundary = true;
                break;
            }
            searchPos++;
        }
        if (wordBoundary) {
            score = 800;
        } else {
            // Tier 4: position-based (max 700, minus 10 per character of distance)
            score = std::max(1, 700 - (int)firstPos * 10);
        }
    }

    // Frequency bonus: extra +50 per additional occurrence after the first
    size_t countPos = 0;
    int occurrences = 0;
    while ((countPos = nameLower.find(queryLower, countPos)) != std::string::npos) {
        occurrences++;
        countPos += queryLower.size();
    }
    if (occurrences > 1) score += (occurrences - 1) * 50;

    // Author bonus: +100 if query appears in author name too
    std::string authorLower = result.Author;
    std::transform(authorLower.begin(), authorLower.end(), authorLower.begin(), ::tolower);
    if (!authorLower.empty() && authorLower.find(queryLower) != std::string::npos) score += 100;

    return std::max(score, 1);
}

void SettingsUI::RebuildDisplayList()
{
    std::string query = localFilterBuf;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    // Score and filter
    std::vector<std::pair<int, size_t>> scored;
    scored.reserve(cachedResultList.size());
    for (size_t i = 0; i < cachedResultList.size(); ++i) {
        int s = ScoreResult(query, cachedResultList[i]);
        if (s > 0) scored.push_back({s, i});
    }

    // Sort descending by score
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    // Build display list
    displayResultList.clear();
    displayResultList.reserve(scored.size());
    for (auto& [score, idx] : scored)
        displayResultList.push_back(cachedResultList[idx]);

    // Reset selection — position in list may have shifted
    selectedBrowserIndex = -1;
}

void SettingsUI::RenderWorkshopBrowserTab()
{
    if (!plugin_->workshopDownloader) {
        ImGui::TextDisabled("Workshop downloader not initialized");
        return;
    }

    ImGui::Spacing();

    static bool pathInit = false;
    if (!pathInit) {
        // Use the resolved workshop root (from workshopmaploader.cfg or platform defaults)
        std::string defaultPath;
        if (plugin_->mapManager) {
            auto resolved = plugin_->mapManager->ResolveConfiguredWorkshopRoot();
            if (!resolved.empty() && fs::exists(resolved)) defaultPath = resolved.string();
        }
        if (defaultPath.empty()) defaultPath = plugin_->gameWrapper->GetDataFolder().string() + "\\SuiteSpot\\Workshop";
        strncpy_s(workshopDownloadPathBuf, defaultPath.c_str(), sizeof(workshopDownloadPathBuf) - 1);
        pathInit = true;
    }

    // Download settings — collapsed by default to save space
    if (ImGui::CollapsingHeader("Download Settings")) {
        ImGui::Indent();
        ImGui::Text("Download to:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##WorkshopPath", workshopDownloadPathBuf, IM_ARRAYSIZE(workshopDownloadPathBuf));
        RenderTextureCheck();

        ImGui::SameLine();
        bool autoDl = plugin_->settingsSync->IsAutoDownloadTextures();
        if (ImGui::Checkbox("Auto-Check on Launch", &autoDl)) {
            UI::Helpers::SetCVarSafely("suitespot_auto_download_textures", autoDl ? 1 : 0, plugin_->cvarManager,
                                       plugin_->gameWrapper);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically check for and download missing textures when the game starts.");
        }
        ImGui::Unindent();
        ImGui::Spacing();
    }

    // API Search — fetches from RLMAPS; input width leaves room for Search button
    ImGui::Text("Search Maps:");
    {
        float searchBtnW = ImGui::CalcTextSize("Search").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - searchBtnW - ImGui::GetStyle().ItemSpacing.x);
    }
    bool enterPressed = ImGui::InputTextWithHint("##WorkshopSearch", "Search workshop maps...", workshopSearchBuf,
                                                 IM_ARRAYSIZE(workshopSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    if ((ImGui::Button("Search") || enterPressed) && strlen(workshopSearchBuf) > 0) {
        // Reset local filter when doing a new API search
        memset(localFilterBuf, 0, sizeof(localFilterBuf));
        lastLocalFilter.clear();
        plugin_->workshopDownloader->GetResults(workshopSearchBuf, 1);
    }

    ImGui::SameLine();
    if (plugin_->workshopDownloader->RLMAPS_Searching) {
        if (ImGui::Button("Stop")) {
            plugin_->workshopDownloader->StopSearch();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Searching...");
    } else if (!cachedResultList.empty()) {
        ImGui::Text("%d / %d maps", (int)displayResultList.size(), (int)cachedResultList.size());
    }

    // Local filter — re-ranks already-loaded results in real time, no API call
    if (!cachedResultList.empty()) {
        ImGui::Spacing();
        ImGui::Text("Filter & Rank:");
        {
            float clearBtnW = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float w = strlen(localFilterBuf) > 0
                          ? ImGui::GetContentRegionAvail().x - clearBtnW - ImGui::GetStyle().ItemSpacing.x
                          : -1.0f;
            ImGui::SetNextItemWidth(w);
        }
        ImGui::InputTextWithHint("##LocalFilter", "Filter & rank by name...", localFilterBuf,
                                 IM_ARRAYSIZE(localFilterBuf));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Type to filter and rank results by relevance. Closer matches appear first.");
        if (strlen(localFilterBuf) > 0) {
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                memset(localFilterBuf, 0, sizeof(localFilterBuf));
                lastLocalFilter.clear();
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Download progress bar
    if (plugin_->workshopDownloader->RLMAPS_IsDownloadingWorkshop) {
        float downloaded = static_cast<float>(plugin_->workshopDownloader->RLMAPS_WorkshopDownload_Progress.load());
        float fileSize = static_cast<float>(plugin_->workshopDownloader->RLMAPS_WorkshopDownload_FileSize.load());
        float fraction = (fileSize > 0) ? (downloaded / fileSize) : 0.0f;

        char label[64];
        snprintf(label, sizeof(label), "%.1f / %.1f MB", downloaded / 1048576.0f, fileSize / 1048576.0f);
        ImGui::ProgressBar(fraction, ImVec2(-1, 20), label);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Search results
    RLMAPS_RenderSearchWorkshopResults(workshopDownloadPathBuf);

    // Popups - donor pattern: thread sets flag, render loop opens popup
    if (plugin_->workshopDownloader->UserIsChoosingYESorNO) {
        ImGui::OpenPopup("Download?");
    }
    RenderAcceptDownload();
    RenderInfoPopup("Downloading?",
                    "A download is already running!\\nYou cannot download 2 workshops at the same time.");
    RenderInfoPopup("Exists?", "This directory is not valid!");

    if (plugin_->workshopDownloader->FolderErrorBool) {
        RenderInfoPopup("FolderError", plugin_->workshopDownloader->FolderErrorText.c_str());
    }
}

void SettingsUI::RLMAPS_RenderSearchWorkshopResults(const char* mapspath)
{
    if (!plugin_->workshopDownloader) return;

    // Check if API list has changed
    int currentVersion = plugin_->workshopDownloader->listVersion.load();
    if (currentVersion != lastListVersion) {
        std::lock_guard<std::mutex> lock(plugin_->workshopDownloader->resultsMutex);
        auto& fullList = plugin_->workshopDownloader->RLMAPS_MapResultList;
        cachedResultList.clear();
        for (auto& result : fullList) {
            // Filter out maps that already exist in the download directory
            std::string safeName = plugin_->workshopDownloader->SanitizeMapName(result.Name);
            std::string mapDir = std::string(mapspath);
            if (!mapDir.empty() && (mapDir.back() != '/' && mapDir.back() != '\\')) mapDir += "/";
            mapDir += safeName;
            if (!fs::exists(mapDir)) {
                cachedResultList.push_back(result);
            }
        }
        LOG("UI Synced list. New version: {}, items: {} (filtered from {})", currentVersion, cachedResultList.size(),
            fullList.size());
        lastListVersion = currentVersion;
        RebuildDisplayList(); // Score and sort the freshly-built cache
    }

    // Re-rank if user changed the local filter text
    std::string currentFilter = localFilterBuf;
    if (currentFilter != lastLocalFilter) {
        lastLocalFilter = currentFilter;
        RebuildDisplayList();
    }

    if (displayResultList.empty()) return;

    // Clamp selection
    if (selectedBrowserIndex >= (int)displayResultList.size()) selectedBrowserIndex = (int)displayResultList.size() - 1;

    // Two-panel layout matching Installed Maps design
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::WorkshopBrowserUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::WorkshopBrowserUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup();

    // === LEFT PANEL: Map List ===
    if (ImGui::BeginChild("##BrowserMapList", ImVec2(leftWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        ImGui::TextDisabled("%d maps", (int)displayResultList.size());
        ImGui::Separator();

        for (int i = 0; i < (int)displayResultList.size(); i++) {
            auto& mapResult = displayResultList[i];
            bool isSelected = (i == selectedBrowserIndex);
            bool hasReleases = !mapResult.releases.empty();

            ImGui::PushID(i);

            // Show loading indicator for maps still fetching releases
            if (!hasReleases) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            }

            if (ImGui::Selectable(mapResult.Name.c_str(), isSelected)) {
                selectedBrowserIndex = i;
            }

            if (!hasReleases) {
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT PANEL: Details + Download ===
    if (ImGui::BeginChild("##BrowserMapDetails", ImVec2(rightWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        if (selectedBrowserIndex >= 0 && selectedBrowserIndex < (int)displayResultList.size()) {
            auto& mapResult = displayResultList[selectedBrowserIndex];

            // Preview image
            std::shared_ptr<ImageWrapper> image = nullptr;
            if (!mapResult.ID.empty()) {
                auto it = workshopImageCache.find(mapResult.ID);
                if (it != workshopImageCache.end()) {
                    image = it->second;
                } else if (!mapResult.ImagePath.empty() && mapResult.isImageLoaded) {
                    try {
                        image = std::make_shared<ImageWrapper>(mapResult.ImagePath.string(), false, true);
                        if (workshopImageCache.size() >= 150) workshopImageCache.erase(workshopImageCache.begin());
                        workshopImageCache[mapResult.ID] = image;
                    } catch (...) {}
                }
            }

            if (image && image->GetImGuiTex()) {
                ImGui::Image(image->GetImGuiTex(), ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                                          UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            } else {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p,
                                  ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                         p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT),
                                  ImColor(40, 40, 45, 255), 4.0f);
                dl->AddText(ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH / 2 - 40,
                                   p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT / 2 - 8),
                            ImColor(100, 100, 100, 255), "No Preview");
                ImGui::Dummy(
                    ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH, UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            }

            ImGui::Spacing();

            // Map name
            ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::MAP_NAME_COLOR);
            ImGui::TextWrapped("%s", mapResult.Name.c_str());
            ImGui::PopStyleColor();

            // Author
            if (!mapResult.Author.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::AUTHOR_COLOR);
                ImGui::Text("By: %s", mapResult.Author.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            // Description
            if (!mapResult.Description.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::DESCRIPTION_COLOR);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", mapResult.Description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Download button
            bool hasReleases = !mapResult.releases.empty();
            if (hasReleases) {
                if (ImGui::Button(ICON_FA_DOWNLOAD " Download", ImVec2(0, 26))) {
                    if (!plugin_->workshopDownloader->RLMAPS_IsDownloadingWorkshop && fs::exists(mapspath)) {
                        ImGui::OpenPopup("Releases");
                    } else if (!fs::exists(mapspath)) {
                        ImGui::OpenPopup("Exists?");
                    } else {
                        ImGui::OpenPopup("Downloading?");
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Download this map to your workshop folder");
                }
                RenderReleases(mapResult, mapspath);
            } else {
                ImGui::TextDisabled("Loading releases...");
            }
        } else {
            ImGui::TextDisabled("Select a map from the list");
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

void SettingsUI::RenderReleases(RLMAPS_MapResult mapResult, const char* mapspath)
{
    if (ImGui::BeginPopupModal("Releases", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (int releasesIndex = 0; releasesIndex < mapResult.releases.size(); releasesIndex++) {
            RLMAPS_Release release = mapResult.releases[releasesIndex];

            if (ImGui::Button(release.tag_name.c_str(), ImVec2(0, 20))) {
                if (!plugin_->workshopDownloader->RLMAPS_IsDownloadingWorkshop && fs::exists(mapspath)) {
                    // Donor pattern: spawn thread immediately, it will spin-wait for confirmation
                    auto downloader = plugin_->workshopDownloader;
                    std::string path = std::string(mapspath);
                    std::thread t2([downloader, path, mapResult, release]() {
                        downloader->RLMAPS_DownloadWorkshop(path, mapResult, release);
                    });
                    t2.detach();
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        if (ImGui::Button("Cancel", ImVec2(0, 20))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SettingsUI::RenderAcceptDownload()
{
    if (!plugin_->workshopDownloader) return;

    RenderYesNoPopup(
        "Download?", "Do you really want to download?\nYou'll not be able to cancel if you start it.",
        [this]() {
            // User confirmed - signal the waiting thread
            plugin_->workshopDownloader->AcceptTheDownload = true;
            plugin_->workshopDownloader->UserIsChoosingYESorNO = false;
            ImGui::CloseCurrentPopup();
        },
        [this]() {
            // User cancelled - signal the waiting thread
            plugin_->workshopDownloader->AcceptTheDownload = false;
            plugin_->workshopDownloader->UserIsChoosingYESorNO = false;
            ImGui::CloseCurrentPopup();
        });
}

void SettingsUI::RenderYesNoPopup(const char* popupName, const char* label, std::function<void()> yesFunc,
                                  std::function<void()> noFunc)
{
    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", label);
        ImGui::NewLine();

        CenterNextItem(0.0f);
        ImGui::BeginGroup();
        {
            if (ImGui::Button("YES", ImVec2(0, 25.0f))) {
                try {
                    yesFunc();
                } catch (const std::exception& ex) {
                    LOG("Popup error: {}", ex.what());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("NO", ImVec2(0, 25.0f))) {
                noFunc();
            }
            ImGui::EndGroup();
        }

        ImGui::EndPopup();
    }
}

void SettingsUI::RenderInfoPopup(const char* popupName, const char* label)
{
    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", label);
        ImGui::NewLine();
        CenterNextItem(0.0f);
        if (ImGui::Button("OK", ImVec2(0, 25.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SettingsUI::CenterNextItem(float itemWidth)
{
    auto windowWidth = ImGui::GetWindowSize().x;
    ImGui::SetCursorPosX((windowWidth - itemWidth) * 0.5f);
}

std::string SettingsUI::LimitTextSize(std::string str, float maxTextSize)
{
    while (ImGui::CalcTextSize(str.c_str()).x > maxTextSize) {
        if (str.empty()) break;
        str = str.substr(0, str.size() - 1);
    }
    return str;
}
void SettingsUI::RenderTextureCheck()
{
    if (!plugin_->textureDownloader) return;

    if (ImGui::Button("Check Textures")) {
        showTexturePopup = true;
        ImGui::OpenPopup("DownloadTextures");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Check for missing workshop textures and install them");
    }

    if (showTexturePopup) {
        std::vector<std::string> missing = plugin_->textureDownloader->CheckMissingTextures();
        RenderDownloadTexturesPopup(missing);
    }
}

void SettingsUI::RenderDownloadTexturesPopup(const std::vector<std::string>& missingFiles)
{
    if (ImGui::BeginPopupModal("DownloadTextures", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!missingFiles.empty()) {
            ImGui::Text("It seems like the workshop textures aren't installed.");
            ImGui::Text("You can still play without them but some maps will have white/weird textures.");

            if (plugin_->textureDownloader->isDownloading) {
                ImGui::Separator();
                ImGui::Text("Downloading... %d%%", plugin_->textureDownloader->downloadProgress.load());
                ImGui::ProgressBar(plugin_->textureDownloader->downloadProgress.load() / 100.0f, ImVec2(300, 20));
                ImGui::Separator();
            }

            ImGui::NewLine();

            if (ImGui::BeginChild("##MissingFiles", ImVec2(300, 150), true)) {
                ImGui::Text("Missing Files (%d):", (int)missingFiles.size());
                ImGui::Separator();
                for (const auto& file : missingFiles) {
                    ImGui::Text("%s", file.c_str());
                }
                ImGui::EndChild();
            }

            ImGui::NewLine();

            if (ImGui::Button("Download & Install", ImVec2(0, 25)) && !plugin_->textureDownloader->isDownloading) {
                plugin_->textureDownloader->DownloadAndInstallTextures();
            }

            ImGui::SameLine();

            if (ImGui::Button("Close", ImVec2(0, 25))) {
                showTexturePopup = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Workshop textures are installed!");
            ImGui::NewLine();
            if (ImGui::Button("OK", ImVec2(0, 25))) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}