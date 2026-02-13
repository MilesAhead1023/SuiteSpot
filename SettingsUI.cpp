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

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>

SettingsUI::SettingsUI(SuiteSpot* plugin) : plugin_(plugin) {}

void SettingsUI::RenderMainSettingsWindow() {
    if (!plugin_) {
        return;
    }

    ImGui::SetWindowFontScale(UI::FONT_SCALE);

    // Header with metadata and Load Now button
    ImGui::BeginGroup();
    ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "By: Flicks Creations");
    std::string ver = "Version: " + std::string(plugin_version);
    ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "%s", ver.c_str());
    {
        // Format __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss") into "Mmm. dd, yyyy h:mm:ss AM/PM"
        char buildDate[] = __DATE__;  // e.g. "Feb 12 2026"
        char buildTime[] = __TIME__;  // e.g. "23:50:30"
        int hour = (buildTime[0] - '0') * 10 + (buildTime[1] - '0');
        const char* ampm = hour >= 12 ? "PM" : "AM";
        int hour12 = hour % 12;
        if (hour12 == 0) hour12 = 12;
        // Month is first 3 chars, day starts at index 4, year starts at index 7
        char day[3] = { buildDate[4] == ' ' ? '0' : buildDate[4], buildDate[5], '\0' };
        ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR,
            "Built: %.3s. %s, %.4s %d:%.2s:%.2s %s",
            buildDate, day, buildDate + 7,
            hour12, buildTime + 3, buildTime + 6, ampm);
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

            float fontSize = font->FontSize;  // Native size — crisp rendering
            ImVec2 clockSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, clockBuf);
            ImVec2 windowPos = ImGui::GetWindowPos();
            float windowWidth = ImGui::GetWindowWidth();
            ImVec2 groupSize = ImGui::GetItemRectSize();  // size of the preceding group
            ImVec2 groupMin = ImGui::GetItemRectMin();     // top-left of the group
            float clockX = windowPos.x + (windowWidth - clockSize.x) * 0.5f;
            float clockY = groupMin.y + (groupSize.y - clockSize.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(font, fontSize, ImVec2(clockX, clockY),
                IM_COL32(255, 255, 255, 255), clockBuf);
        }
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
    if (ImGui::Button("LOAD NOW", ImVec2(130, 26))) {
        int mapType = plugin_->GetMapType();
        SuiteSpot* p = plugin_;
        bool issuedLoad = false;

        if (mapType == 0) { // Freeplay
            std::string code = p->GetCurrentFreeplayCode();
            if (!code.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Freeplay: {})", code);
                p->gameWrapper->SetTimeout([p, code](GameWrapper* gw) {
                    p->cvarManager->executeCommand("load_freeplay " + code);
                }, 0.0f);
                statusMessage.ShowSuccess("Loading Freeplay", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        } else if (mapType == 1) { // Training
            std::string code = p->settingsSync->GetQuickPicksSelectedCode();
            if (code.empty()) code = p->GetCurrentTrainingCode();

            if (!code.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Training: {})", code);
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(code);
                p->gameWrapper->SetTimeout([p, code](GameWrapper* gw) {
                    p->cvarManager->executeCommand("load_training " + code);
                }, 0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        } else if (mapType == 2) { // Workshop
            std::string path = p->GetCurrentWorkshopPath();
            if (!path.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Workshop: {})", path);
                p->gameWrapper->SetTimeout([p, path](GameWrapper* gw) {
                    p->cvarManager->executeCommand("load_workshop \"" + path + "\"");
                }, 0.0f);
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
        std::string queueDelayStr = std::to_string(delayQueueSecValue) + "s";
        std::string mapDelayStr = "0s";
        const ImVec4 white = UI::SettingsUI::STATUS_SEPARATOR_COLOR;

        // Get current selection and appropriate delay
        if (mapTypeValue == 0) {
            // Find freeplay map by code
            auto it = std::find_if(RLMaps.begin(), RLMaps.end(),
                [&](const MapEntry& e) { return e.code == currentFreeplayCode; });
            if (it != RLMaps.end()) {
                currentMap = it->name;
            }
            mapDelayStr = std::to_string(delayFreeplaySecValue) + "s";
        } else if (mapTypeValue == 1) {
            mapDelayStr = std::to_string(delayTrainingSecValue) + "s";

            // Get packs from manager for consistent lookup
            const auto& trainingPacks = plugin_->trainingPackMgr ?
                plugin_->trainingPackMgr->GetPacks() : RLTraining;

            std::string targetCode = quickPicksSelectedCode;
            if (targetCode.empty()) targetCode = currentTrainingCode;
            
            const TrainingEntry* targetPack = plugin_->trainingPackMgr->GetPackByCode(targetCode);
            if (targetPack) {
                currentMap = targetPack->name;
            } else if (!targetCode.empty()) {
                currentMap = targetCode + " (custom)";
            } else {
                currentMap = "<none selected>";
            }
            
            mapDelayStr = "with " + std::to_string(delayTrainingSecValue) + "s delay";
        } else if (mapTypeValue == 2) {
            // Find workshop map by path
            auto it = std::find_if(RLWorkshop.begin(), RLWorkshop.end(),
                [&](const WorkshopEntry& e) { return e.filePath == currentWorkshopPath; });
            if (it != RLWorkshop.end()) {
                currentMap = it->name;
            }
            mapDelayStr = std::to_string(delayWorkshopSecValue) + "s";
        }

        // Map mode status
        const ImVec4 green = UI::SettingsUI::STATUS_ENABLED_TEXT_COLOR;
        const ImVec4 red   = UI::SettingsUI::STATUS_DISABLED_TEXT_COLOR;
        std::string modeText = "Mode: " + std::string(modeNames[mapTypeValue]);
        
        int currentDelay = (mapTypeValue == 0) ? delayFreeplaySecValue : (mapTypeValue == 1 ? delayTrainingSecValue : delayWorkshopSecValue);
        if (currentDelay > 0) {
            modeText += " Delayed: " + mapDelayStr;
        }

        ImGui::TextColored(green, "%s", modeText.c_str());
        ImGui::SameLine();
        ImGui::TextColored(white, "|");
        ImGui::SameLine();
        ImGui::TextColored(green, "Map: %s", currentMap.c_str());
        ImGui::SameLine();
        ImGui::TextColored(white, "|");
        ImGui::SameLine();
        const ImVec4 queueColor = autoQueueValue ? green : red;
        if (delayQueueSecValue > 0) {
            ImGui::TextColored(queueColor, "Next Match Queue Delayed: %s", queueDelayStr.c_str());
        } else {
            ImGui::TextColored(queueColor, "Next Match Queue");
        }
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

    // Main tab bar
    if (ImGui::BeginTabBar("SuiteSpotTabs", ImGuiTabBarFlags_None)) {

        // ===== MAP SELECT TAB =====
        if (ImGui::BeginTabItem("Map Select")) {
            ImGui::Spacing();

            // 1) Unified Header: Auto-Queue Toggle | Queue Delay | Map Delay
            ImGui::Columns(2, "MapSelectHeaderCols", false);
            ImGui::SetColumnWidth(0, 150.0f);
            
            // Auto-Queue
            UI::Helpers::CheckboxWithCVar("Auto-Queue", autoQueueValue, "suitespot_auto_queue",
                plugin_->cvarManager, plugin_->gameWrapper, "Automatically queue into the next match after the current match ends.");
            ImGui::NextColumn();
            ImGui::NextColumn(); // Skip right column

            // Queue Delay
            ImGui::Text("Queue Delay");
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(-1);
            UI::Helpers::InputIntWithRange("##QueueDelay", delayQueueSecValue,
                UI::SettingsUI::DELAY_QUEUE_MIN_SECONDS, UI::SettingsUI::DELAY_QUEUE_MAX_SECONDS,
                0.0f, "suitespot_delay_queue_sec", plugin_->cvarManager,
                plugin_->gameWrapper, "Wait before auto-queuing.", nullptr);
            ImGui::NextColumn();

            // Map Delay (Context-sensitive)
            int* currentMapDelayValue = &delayFreeplaySecValue;
            const char* currentMapDelayCVar = "suitespot_delay_freeplay_sec";
            const char* mapDelayTooltip = "Wait before loading Freeplay.";

            if (mapTypeValue == 1) { // Training
                currentMapDelayValue = &delayTrainingSecValue;
                currentMapDelayCVar = "suitespot_delay_training_sec";
                mapDelayTooltip = "Wait before loading Training.";
            } else if (mapTypeValue == 2) { // Workshop
                currentMapDelayValue = &delayWorkshopSecValue;
                currentMapDelayCVar = "suitespot_delay_workshop_sec";
                mapDelayTooltip = "Wait before loading Workshop.";
            }

            ImGui::Text("Map Delay");
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(-1);
            UI::Helpers::InputIntWithRange("##MapDelay", *currentMapDelayValue,
                0, 300, 0.0f, currentMapDelayCVar, plugin_->cvarManager,
                plugin_->gameWrapper, mapDelayTooltip, nullptr);
            ImGui::NextColumn();

            ImGui::Columns(1); // Reset
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 2) Map Selection Logic
            RenderMapSelectionTab(mapTypeValue, currentFreeplayCode,
                currentTrainingCode, currentWorkshopPath, delayFreeplaySecValue,
                delayTrainingSecValue, delayWorkshopSecValue, delayQueueSecValue);

            ImGui::EndTabItem();
        }

        // ===== LOADOUT MANAGEMENT TAB =====
        if (ImGui::BeginTabItem("Loadout Management")) {
            if (plugin_->loadoutUI) {
                plugin_->loadoutUI->RenderLoadoutControls();
            }
            ImGui::EndTabItem();
        }
        
        // ===== WORKSHOP BROWSER TAB =====
        if (ImGui::BeginTabItem("Workshop Browser")) {
            RenderWorkshopBrowserTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void SettingsUI::RenderGeneralTab(bool& enabledValue, int& mapTypeValue) {
    ImGui::Columns(2, "GeneralTabCols", false); // Invisible columns
    
    // Col 1: Enable
    UI::Helpers::CheckboxWithCVar("Enable SuiteSpot", enabledValue, "suitespot_enabled",
        plugin_->cvarManager, plugin_->gameWrapper, "Enable/disable all SuiteSpot auto-loading and queuing features");
    
    ImGui::NextColumn();
    
    // Col 2: Map Mode
    ImGui::TextUnformatted("Map Mode:");
    ImGui::SameLine();
    
    const char* mapLabels[] = {"Freeplay", "Training", "Workshop"};
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine(0, UI::SettingsUI::MAP_TYPE_RADIO_BUTTON_SPACING);
        if (ImGui::RadioButton(mapLabels[i], mapTypeValue == i)) {
            mapTypeValue = i;
            LOG("SuiteSpot UI: User switched Map Mode to {}", mapLabels[i]);
            UI::Helpers::SetCVarSafely("suitespot_map_type", mapTypeValue, plugin_->cvarManager, plugin_->gameWrapper);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Choose which map type loads after matches:\nFreeplay = Official | Training = Custom Packs | Workshop = Modded Maps");
    }
    
    ImGui::Columns(1); // Reset
    ImGui::Spacing();

    bool gameSpeedFixEnabled = plugin_->settingsSync
        ? plugin_->settingsSync->IsTrainingGameSpeedFixEnabled()
        : true;
    if (UI::Helpers::CheckboxWithCVar(
        "Fix Training Game Speed",
        gameSpeedFixEnabled,
        "suitespot_fix_training_gamespeed",
        plugin_->cvarManager,
        plugin_->gameWrapper,
        "Sync in-game training speed with sv_soccar_gamespeed in training playlists."
    )) {
        LOG("Fix Training Game Speed toggled to {}", gameSpeedFixEnabled ? "ON" : "OFF");
    }
}

void SettingsUI::RenderMapSelectionTab(int mapTypeValue,
    std::string& currentFreeplayCode,
    std::string& currentTrainingCode,
    std::string& currentWorkshopPath,
    int& delayFreeplaySecValue,
    int& delayTrainingSecValue,
    int& delayWorkshopSecValue,
    int& delayQueueSecValue) {
    ImGui::TextUnformatted("Map Selection:");
    ImGui::Spacing();

    if (mapTypeValue == 0) {
        RenderFreeplayMode(currentFreeplayCode);
    } else if (mapTypeValue == 1) {
        RenderTrainingMode(0, currentTrainingCode);
    } else if (mapTypeValue == 2) {
        RenderWorkshopMode(currentWorkshopPath);
    }
}

void SettingsUI::RenderFreeplayMode(std::string& currentFreeplayCode) {
// Initialize to first map if empty and maps are available
if (currentFreeplayCode.empty() && !RLMaps.empty()) {
    currentFreeplayCode = RLMaps[0].code;
    plugin_->settingsSync->SetCurrentFreeplayCode(currentFreeplayCode);
    plugin_->cvarManager->getCvar("suitespot_current_freeplay_code").setValue(currentFreeplayCode);
}

// Find current selection index for display
int currentIndex = 0;
if (!currentFreeplayCode.empty()) {
    for (int i = 0; i < (int)RLMaps.size(); i++) {
        if (RLMaps[i].code == currentFreeplayCode) {
            currentIndex = i;
            break;
        }
    }
}

const char* freeplayLabel = RLMaps.empty() ? "<none>" : RLMaps[currentIndex].name.c_str();
    
    ImGui::Columns(2, "FreeplayCols", false);
    ImGui::SetColumnWidth(0, 150.0f);
    
    ImGui::Text("Freeplay Map");
    ImGui::NextColumn();
    
    ImGui::SetNextItemWidth(-1);
    if (UI::Helpers::ComboWithTooltip("##FreeplayMap", freeplayLabel,
        "Select which stadium to load after matches", -1.0f)) {
        ImGuiListClipper clipper;
        clipper.Begin((int)RLMaps.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                bool selected = (RLMaps[row].code == currentFreeplayCode);
                if (ImGui::Selectable(RLMaps[row].name.c_str(), selected)) {
                    currentFreeplayCode = RLMaps[row].code;
                    LOG("SuiteSpot UI: User selected Freeplay map: {} ({})", RLMaps[row].name, currentFreeplayCode);
                    plugin_->settingsSync->SetCurrentFreeplayCode(currentFreeplayCode);
                    plugin_->cvarManager->getCvar("suitespot_current_freeplay_code").setValue(currentFreeplayCode);
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Columns(1);
}

void SettingsUI::RenderTrainingMode(int trainingModeValue, std::string& currentTrainingCode) {
    RenderSinglePackMode(currentTrainingCode);

    ImGui::Spacing();

    if (ImGui::Button("Open Training Pack Browser", ImVec2(250, 30))) {
        SuiteSpot* p = plugin_;
        p->gameWrapper->SetTimeout([p](GameWrapper* gw) {
            p->cvarManager->executeCommand("togglemenu suitespot_browser");
        }, 0.0f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open the full training pack browser to manage bags and packs");
    }
}

void SettingsUI::RenderWorkshopMode(std::string& currentWorkshopPath) {
    // Header with Refresh button
    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Local Workshop Maps");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);
    if (ImGui::Button("Refresh", ImVec2(70, 0))) {
        plugin_->LoadWorkshopMaps();
        selectedWorkshopIndex = -1;  // Reset selection
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rescan workshop folders for maps");
    }
    ImGui::Spacing();

    // Check if we have any maps
    if (RLWorkshop.empty()) {
        ImGui::TextColored(UI::WorkshopBrowserUI::NO_MAPS_COLOR,
            "No workshop maps found.");
        ImGui::TextDisabled("Maps are discovered from:");
        ImGui::BulletText("WorkshopMapLoader configured path");
        ImGui::BulletText("Epic Games install mods folder");
        ImGui::BulletText("Steam install mods folder");
        ImGui::Spacing();
        ImGui::TextDisabled("Download maps from the Workshop Browser tab above.");
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
        ImGui::TextDisabled("%d maps", (int)RLWorkshop.size());
        ImGui::Separator();

        for (int i = 0; i < (int)RLWorkshop.size(); i++) {
            const auto& entry = RLWorkshop[i];
            bool isSelected = (i == selectedWorkshopIndex);
            bool isCurrentAutoLoad = (entry.filePath == currentWorkshopPath);

            ImGui::PushID(i);

            // Show marker if this is the current auto-load selection
            if (isCurrentAutoLoad) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::SELECTED_BADGE_COLOR);
                ImGui::Text(">");
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            if (ImGui::Selectable(entry.name.c_str(), isSelected, ImGuiSelectableFlags_None)) {
                selectedWorkshopIndex = i;
                // FIX: Update CVar immediately when selection changes (not just on explicit button click)
                // This synchronizes selectedWorkshopIndex with currentWorkshopPath so "Load Now" works
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
                p->gameWrapper->SetTimeout([p, path](GameWrapper* gw) {
                    p->cvarManager->executeCommand("load_workshop \"" + path + "\"");
                }, 0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) {
                    p->cvarManager->executeCommand("togglemenu settings");
                }
            }

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
                drawList->AddText(
                    ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH / 2 - 40,
                           p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT / 2 - 8),
                    ImColor(100, 100, 100, 255), "No Preview");
                ImGui::Dummy(ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                    UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
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
                if (ImGui::Button("Select for Post-Match", ImVec2(180, 26))) {
                    plugin_->settingsSync->SetCurrentWorkshopPath(selectedMap.filePath);
                    plugin_->cvarManager->getCvar("suitespot_current_workshop_path").setValue(selectedMap.filePath);
                    currentWorkshopPath = selectedMap.filePath;
                    statusMessage.ShowSuccess("Workshop map selected", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Set this map to load after matches end");
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Load Now", ImVec2(100, 26))) {
                SuiteSpot* p = plugin_;
                std::string path = selectedMap.filePath;
                p->gameWrapper->SetTimeout([p, path](GameWrapper* gw) {
                    p->cvarManager->executeCommand("load_workshop \"" + path + "\"");
                }, 0.0f);
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

void SettingsUI::RenderSinglePackMode(std::string& currentTrainingCode) {
// Toggle between Flicks Picks and Your Favorites
int listType = plugin_->settingsSync->GetQuickPicksListType();
    
ImGui::TextUnformatted("List Type:");
ImGui::SameLine();
    
if (ImGui::RadioButton("Flicks Picks", listType == 0)) {
    UI::Helpers::SetCVarSafely("suitespot_quickpicks_list_type", 0, plugin_->cvarManager, plugin_->gameWrapper);
}
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Curated selection of 10 essential training packs");
}
ImGui::SameLine();
if (ImGui::RadioButton("Your Favorites", listType == 1)) {
    UI::Helpers::SetCVarSafely("suitespot_quickpicks_list_type", 1, plugin_->cvarManager, plugin_->gameWrapper);
}
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Your most-used training packs based on load history");
}

ImGui::Spacing();
ImGui::Separator();
ImGui::Spacing();

// Display header based on list type
if (listType == 0) {
    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Flicks Picks");
} else {
    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Your Favorites");
}
ImGui::SameLine();
ImGui::TextDisabled("(Select post-match pack)");

std::vector<std::string> quickPicks = GetQuickPicksList();
std::string selectedCode = plugin_->settingsSync->GetQuickPicksSelectedCode();
    
    // If nothing selected, default to the first one in the list
    if (selectedCode.empty() && !quickPicks.empty()) {
        selectedCode = quickPicks[0];
        plugin_->settingsSync->SetQuickPicksSelected(selectedCode);
        plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(selectedCode);
    }

    if (ImGui::BeginChild("QuickPicksList", ImVec2(UI::QuickPicksUI::TABLE_WIDTH, UI::QuickPicksUI::TABLE_HEIGHT), true)) {
        for (const auto& code : quickPicks) {
            std::string name = "Unknown Pack";
            int shots = 0;
            std::string description = "";
            bool found = false;

            // 1. Try to find in loaded cache
            const auto& trainingPacks = plugin_->trainingPackMgr ? plugin_->trainingPackMgr->GetPacks() : RLTraining;
            auto it = std::find_if(trainingPacks.begin(), trainingPacks.end(), [&](const TrainingEntry& e) { return e.code == code; });
            
            if (it != trainingPacks.end()) {
                name = it->name;
                shots = it->shotCount;
                description = it->staffComments.empty() ? it->notes : it->staffComments;
                found = true;
            } else {
                // 2. Try to find in DefaultPacks (Hardcoded fallback for first run)
                for (const auto& defPack : DefaultPacks::FLICKS_PICKS) {
                    if (defPack.code == code) {
                        name = defPack.name;
                        shots = defPack.shotCount;
                        description = defPack.description;
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                bool isSelected = (code == selectedCode);
                
                // Add top padding
                ImGui::Dummy(ImVec2(0, 4.0f));
                
                ImGui::PushID(code.c_str());
                if (ImGui::RadioButton("##select", isSelected)) {
                    selectedCode = code;
                    LOG("SuiteSpot UI: User selected Training pack: {} ({})", name, code);
                    plugin_->settingsSync->SetQuickPicksSelected(code);
                    plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(code);
                }
                ImGui::SameLine();
                
                // Name and Shot Count
                float availWidth = ImGui::GetContentRegionAvail().x;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White for name
                ImGui::Text("%s", name.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(availWidth - 80.0f); 
                ImGui::TextDisabled("| %d shots", shots);

                // Description (Indented and Wrapped)
                if (!description.empty()) {
                    ImGui::Indent(28.0f); 
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Darker gray for description
                    ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionWidth() - 10.0f);
                    ImGui::TextUnformatted(description.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                    ImGui::Unindent(28.0f);
                }
                
                // Add bottom padding
                ImGui::Dummy(ImVec2(0, 4.0f));
                
                ImGui::Separator();
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

std::vector<std::string> SettingsUI::GetQuickPicksList() {
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

    // Your Favorites mode - use usage tracker
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
void SettingsUI::RenderWorkshopBrowserTab() {
    if (!plugin_->workshopDownloader) {
        ImGui::TextDisabled("Workshop downloader not initialized");
        return;
    }

    ImGui::Spacing();
    
    static bool pathInit = false;
    if (!pathInit) {
        std::string defaultPath = plugin_->gameWrapper->GetDataFolder().string() + "\\SuiteSpot\\Workshop";
        strncpy_s(workshopDownloadPathBuf, defaultPath.c_str(), sizeof(workshopDownloadPathBuf) - 1);
        pathInit = true;
    }

    // Download destination path
    ImGui::Text("Download to:");
    ImGui::SetNextItemWidth(400.0f);
    ImGui::InputText("##WorkshopPath", workshopDownloadPathBuf, IM_ARRAYSIZE(workshopDownloadPathBuf));
    
    ImGui::SameLine();
    RenderTextureCheck();

    ImGui::SameLine();
    bool autoDl = plugin_->settingsSync->IsAutoDownloadTextures();
    if (ImGui::Checkbox("Auto-Check on Launch", &autoDl)) {
        UI::Helpers::SetCVarSafely("suitespot_auto_download_textures", autoDl ? 1 : 0, plugin_->cvarManager, plugin_->gameWrapper);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically check for and download missing textures when the game starts.");
    }

    ImGui::Spacing();
    
    // Search bar
    ImGui::Text("Search Maps:");
    ImGui::SetNextItemWidth(400.0f);
    bool enterPressed = ImGui::InputText("##WorkshopSearch", workshopSearchBuf, IM_ARRAYSIZE(workshopSearchBuf), 
                                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    
    if ((ImGui::Button("Search") || enterPressed) && strlen(workshopSearchBuf) > 0) {
        plugin_->workshopDownloader->GetResults(workshopSearchBuf, 1);
    }
    
    ImGui::SameLine();
    if (plugin_->workshopDownloader->RLMAPS_Searching) {
        if (ImGui::Button("Stop")) {
            plugin_->workshopDownloader->StopSearch();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Searching...");
    } else if (plugin_->workshopDownloader->RLMAPS_NumberOfMapsFound > 0) {
        ImGui::Text("%d maps found", plugin_->workshopDownloader->RLMAPS_NumberOfMapsFound.load());
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Search results
    RLMAPS_RenderSearchWorkshopResults(workshopDownloadPathBuf);
    
    // Popups
    RenderAcceptDownload();
    RenderInfoPopup("Downloading?", "A download is already running!\\nYou cannot download 2 workshops at the same time.");
    RenderInfoPopup("Exists?", "This directory is not valid!");
    
    if (plugin_->workshopDownloader->FolderErrorBool) {
        RenderInfoPopup("FolderError", plugin_->workshopDownloader->FolderErrorText.c_str());
    }
}

void SettingsUI::RLMAPS_RenderSearchWorkshopResults(const char* mapspath) {
    if (!plugin_->workshopDownloader) return;
    
    // Check if list has changed
    int currentVersion = plugin_->workshopDownloader->listVersion.load();
    if (currentVersion != lastListVersion) {
        std::lock_guard<std::mutex> lock(plugin_->workshopDownloader->resultsMutex);
        cachedResultList = plugin_->workshopDownloader->RLMAPS_MapResultList;
        LOG("UI Synced list. New version: {}, items: {}", currentVersion, cachedResultList.size());
        lastListVersion = currentVersion;
    }
    
    if (cachedResultList.empty()) return;

    // Lazy load images (only if not loaded)
    // We only check loaded status here, actual loading happens in background or when flagged
    // We do NOT perform FS checks every frame.
    // The downloader updates the image pointer when ready.
    
    // OPTIMIZATION: Removed O(N²) sync loop that matched cached images back to original list
    // This was causing quadratic CPU burn. The original list will get fresh images on next search.
    
    if (ImGui::BeginChild("##SearchResults", ImVec2(0, 500), true)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        int columns = 4;
        float cardWidth = 190.0f;
        float cardHeight = 260.0f;

        // OPTIMIZATION: Use ImGuiListClipper to render only visible cards (virtual scrolling)
        // This reduces rendering from O(50+) items to O(visible~15) items, restoring 60 FPS
        ImGuiListClipper clipper;
        int totalItems = (int)cachedResultList.size();
        int itemsPerRow = columns;
        int totalRows = (totalItems + itemsPerRow - 1) / itemsPerRow; // Ceiling division

        // Specify row height to fix first-row rendering issue
        clipper.Begin(totalRows, cardHeight);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                for (int col = 0; col < columns; col++) {
                    int i = row * columns + col;
                    if (i >= totalItems) break;

                    if (col > 0) ImGui::SameLine();
                    RLMAPS_RenderAResult(i, drawList, mapspath);
                }
            }
        }
        clipper.End();
    }
    ImGui::EndChild();
}

void SettingsUI::RLMAPS_RenderAResult(int i, ImDrawList* drawList, const char* mapspath) {
    if (!plugin_->workshopDownloader) return;
    
    if (i >= cachedResultList.size()) return;
    
    ImGui::PushID(i);
    
    RLMAPS_MapResult& mapResult = cachedResultList[i];
    std::string mapName = mapResult.Name;
    std::string mapDescription = mapResult.Description;
    std::string mapAuthor = mapResult.Author;
    
    ImGui::BeginChild("##RlmapsResult", ImVec2(190.0f, 260.0f));
    {
        ImGui::BeginGroup();
        {
            ImVec2 TopCornerLeft = ImGui::GetCursorScreenPos();
            ImVec2 RectFilled_p_max = ImVec2(TopCornerLeft.x + 190.0f, TopCornerLeft.y + 260.0f);
            ImVec2 ImageP_Min = ImVec2(TopCornerLeft.x + 6.0f, TopCornerLeft.y + 6.0f);
            ImVec2 ImageP_Max = ImVec2(TopCornerLeft.x + 184.0f, TopCornerLeft.y + 179.0f);
            
            drawList->AddRectFilled(TopCornerLeft, RectFilled_p_max, ImColor(44, 75, 113, 255), 5.0f, 15);
            drawList->AddRect(ImageP_Min, ImageP_Max, ImColor(255, 255, 255, 255), 0, 15, 2.0f);

            // Use persistent image cache (keyed by map ID) to survive list refreshes
            std::shared_ptr<ImageWrapper> image = nullptr;
            if (!mapResult.ID.empty()) {
                auto it = workshopImageCache.find(mapResult.ID);
                if (it != workshopImageCache.end()) {
                    image = it->second;
                } else if (!mapResult.ImagePath.empty() && mapResult.isImageLoaded) {
                    // Load image and store in cache
                    try {
                        image = std::make_shared<ImageWrapper>(mapResult.ImagePath.string(), false, true);
                        workshopImageCache[mapResult.ID] = image;
                    } catch (...) {
                        // Failed to load image
                    }
                }
            }

            if (image) {
                try {
                    if (image->GetImGuiTex()) {
                        drawList->AddImage(image->GetImGuiTex(), ImageP_Min, ImageP_Max);
                    }
                } catch (...) {}
            }
            
            std::string GoodMapName = mapName;
            if (ImGui::CalcTextSize(GoodMapName.c_str()).x > 180.0f) {
                GoodMapName = LimitTextSize(GoodMapName, 170.0f) + "...";
            }
            drawList->AddText(ImVec2(TopCornerLeft.x + 4.0f, TopCornerLeft.y + 185.0f), 
                             ImColor(255, 255, 255, 255), GoodMapName.c_str());
            drawList->AddText(ImVec2(TopCornerLeft.x + 4.0f, TopCornerLeft.y + 215.0f), 
                             ImColor(255, 255, 255, 255), ("By " + mapAuthor).c_str());
            
            ImGui::SetCursorScreenPos(ImVec2(TopCornerLeft.x + 4.0f, TopCornerLeft.y + 235.0f));
            
            bool hasReleases = !mapResult.releases.empty();
            
            if (hasReleases) {
                if (ImGui::Button("Download", ImVec2(182, 20))) {
                    if (!plugin_->workshopDownloader->RLMAPS_IsDownloadingWorkshop && 
                        fs::exists(mapspath)) {
                        ImGui::OpenPopup("Releases");
                    } else if (!fs::exists(mapspath)) {
                        ImGui::OpenPopup("Exists?");
                    } else {
                        ImGui::OpenPopup("Downloading?");
                    }
                }
                RenderReleases(mapResult, mapspath);
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button("Loading...", ImVec2(182, 20));
                ImGui::PopStyleVar();
            }
            
            ImGui::EndGroup();
            
            if (ImGui::IsItemHovered()) {
                std::string GoodDescription = mapDescription;
                if (mapDescription.length() > 150) {
                    GoodDescription = mapDescription.substr(0, 150) + "...";
                }
                
                ImGui::BeginTooltip();
                ImGui::Text("Title: %s", mapName.c_str());
                ImGui::Text("By: %s", mapAuthor.c_str());
                ImGui::Text("Description:\\n%s", GoodDescription.c_str());
                ImGui::EndTooltip();
            }
        }
        ImGui::EndChild();
    }
    
    ImGui::PopID();
}

void SettingsUI::RenderReleases(RLMAPS_MapResult mapResult, const char* mapspath) {
    if (ImGui::BeginPopupModal("Releases", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (int releasesIndex = 0; releasesIndex < mapResult.releases.size(); releasesIndex++) {
            RLMAPS_Release release = mapResult.releases[releasesIndex];
            
            if (ImGui::Button(release.tag_name.c_str(), ImVec2(182, 20))) {
                if (!plugin_->workshopDownloader->RLMAPS_IsDownloadingWorkshop && 
                    fs::exists(mapspath)) {
                    // Store pending download info and open confirmation popup
                    hasPendingDownload = true;
                    pendingMapResult = mapResult;
                    pendingRelease = release;
                    pendingDownloadPath = std::string(mapspath);
                    ImGui::CloseCurrentPopup();
                    ImGui::OpenPopup("Download?");
                }
            }
        }
        
        if (ImGui::Button("Cancel", ImVec2(182, 20))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void SettingsUI::RenderAcceptDownload() {
    if (!plugin_->workshopDownloader) return;
    
    RenderYesNoPopup("Download?", 
                     "Do you really want to download?\nYou'll not be able to cancel if you start it.",
                     [this]() {
                         // User confirmed - start the download
                         if (hasPendingDownload) {
                             auto downloader = plugin_->workshopDownloader;
                             std::thread t2([downloader, this]() {
                                 downloader->RLMAPS_DownloadWorkshop(
                                     pendingDownloadPath, pendingMapResult, pendingRelease);
                             });
                             t2.detach();
                             hasPendingDownload = false;
                         }
                         ImGui::CloseCurrentPopup();
                     },
                     [this]() {
                         // User cancelled - clear pending download
                         hasPendingDownload = false;
                         ImGui::CloseCurrentPopup();
                     });
}

void SettingsUI::RenderYesNoPopup(const char* popupName, const char* label, 
                                   std::function<void()> yesFunc, std::function<void()> noFunc) {
    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", label);
        ImGui::NewLine();
        
        CenterNextItem(208.0f);
        ImGui::BeginGroup();
        {
            if (ImGui::Button("YES", ImVec2(100.0f, 25.0f))) {
                try {
                    yesFunc();
                } catch (const std::exception& ex) {
                    LOG("Popup error: {}", ex.what());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("NO", ImVec2(100.0f, 25.0f))) {
                noFunc();
            }
            ImGui::EndGroup();
        }
        
        ImGui::EndPopup();
    }
}

void SettingsUI::RenderInfoPopup(const char* popupName, const char* label) {
    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", label);
        ImGui::NewLine();
        CenterNextItem(100.0f);
        if (ImGui::Button("OK", ImVec2(100.0f, 25.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SettingsUI::CenterNextItem(float itemWidth) {
    auto windowWidth = ImGui::GetWindowSize().x;
    ImGui::SetCursorPosX((windowWidth - itemWidth) * 0.5f);
}

std::string SettingsUI::LimitTextSize(std::string str, float maxTextSize) {
    while (ImGui::CalcTextSize(str.c_str()).x > maxTextSize) {
        if (str.empty()) break;
        str = str.substr(0, str.size() - 1);
    }
    return str;
}
void SettingsUI::RenderTextureCheck() {
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

void SettingsUI::RenderDownloadTexturesPopup(const std::vector<std::string>& missingFiles) {
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

            if (ImGui::Button("Download & Install", ImVec2(140, 25)) && !plugin_->textureDownloader->isDownloading) {
                std::thread t([this]() {
                    plugin_->textureDownloader->DownloadAndInstallTextures();
                });
                t.detach();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Close", ImVec2(100, 25))) {
                showTexturePopup = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Workshop textures are installed!");
            ImGui::NewLine();
            if (ImGui::Button("OK", ImVec2(100, 25))) {
                showTexturePopup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}
