#include "pch.h"
#include "SuiteSpot.h"
#include "MapList.h"
#include "MapManager.h"
#include "SettingsSync.h"
#include "AutoLoadFeature.h"
#include "TrainingPackManager.h"
#include "WorkshopDownloader.h"
#include "SettingsUI.h"
#include "TrainingPackUI.h"
#include "LoadoutUI.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"
#include "bakkesmod/wrappers/GameEvent/TrainingEditorWrapper.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <random>
#include <iomanip>
#include <sstream>
#include <cmath>

/*
 * ======================================================================================
 * SUITESPOT: IMPLEMENTATION DETAILS
 * ======================================================================================
 * 
 * This file contains the actual code for the plugin's lifecycle.
 * 
 * KEY SECTIONS:
 * 1. PERSISTENCE HELPERS: Tiny functions that ask the Managers for data paths (like where 
 *    to find Workshop maps).
 * 2. EVENT HOOKS: The `GameEndedEvent` function is the heartbeat of the automation. 
 *    It waits for the match to finish, then triggers the `AutoLoadFeature`.
 * 3. LOADING (onLoad):
 *    - Creates all the "Managers" (tools for Maps, Settings, Packs).
 *    - Registers the "Browser Window" logic (via togglemenu).
 *    - Hooks into the game events.
 * 4. RENDERING:
 *    - Handled natively by BakkesMod's PluginWindow interface.
 *    - Uses a custom OnClose override to ensure standalone windows stay persistent
 *      when the main settings menu is closed.
 */

// ===== SuiteSpot persistence helpers =====
std::filesystem::path SuiteSpot::GetDataRoot() const
{
    return mapManager ? mapManager->GetDataRoot() : std::filesystem::path();
}

std::filesystem::path SuiteSpot::GetSuiteTrainingDir() const
{
    return mapManager ? mapManager->GetSuiteTrainingDir() : std::filesystem::path();
}

void SuiteSpot::EnsureDataDirectories() const
{
    if (mapManager) {
        mapManager->EnsureDataDirectories();
    }
}

std::filesystem::path SuiteSpot::GetWorkshopLoaderConfigPath() const
{
    return mapManager ? mapManager->GetWorkshopLoaderConfigPath() : std::filesystem::path();
}

std::filesystem::path SuiteSpot::ResolveConfiguredWorkshopRoot() const
{
    return mapManager ? mapManager->ResolveConfiguredWorkshopRoot() : std::filesystem::path();
}

void SuiteSpot::DiscoverWorkshopInDir(const std::filesystem::path& dir)
{
    if (mapManager) {
        mapManager->DiscoverWorkshopInDir(dir, SuiteWorkshop);
    }
}

void SuiteSpot::LoadWorkshopMaps()
{
    if (mapManager) {
        // Load workshop maps without passing an index - the path-based selection persists automatically
        int unused = 0;
        mapManager->LoadWorkshopMaps(SuiteWorkshop, unused);
    }
}

// ===== TRAINING PACK UPDATE INTEGRATION =====
bool SuiteSpot::IsEnabled() const
{
    return settingsSync ? settingsSync->IsEnabled() : false;
}

bool SuiteSpot::IsAutoQueueEnabled() const
{
    return settingsSync ? settingsSync->IsAutoQueue() : false;
}

bool SuiteSpot::IsTrainingGameSpeedFixEnabled() const
{
    return settingsSync ? settingsSync->IsTrainingGameSpeedFixEnabled() : true;
}

int SuiteSpot::GetMapType() const
{
    return settingsSync ? settingsSync->GetMapType() : 0;
}

int SuiteSpot::GetDelayQueueSec() const
{
    return settingsSync ? settingsSync->GetDelayQueueSec() : 0;
}

int SuiteSpot::GetDelayFreeplaySec() const
{
    return settingsSync ? settingsSync->GetDelayFreeplaySec() : 0;
}

int SuiteSpot::GetDelayTrainingSec() const
{
    return settingsSync ? settingsSync->GetDelayTrainingSec() : 0;
}

int SuiteSpot::GetDelayWorkshopSec() const
{
    return settingsSync ? settingsSync->GetDelayWorkshopSec() : 0;
}

std::string SuiteSpot::GetCurrentFreeplayCode() const
{
    return settingsSync ? settingsSync->GetCurrentFreeplayCode() : "";
}

std::string SuiteSpot::GetCurrentTrainingCode() const
{
    return settingsSync ? settingsSync->GetCurrentTrainingCode() : "";
}

std::string SuiteSpot::GetCurrentWorkshopPath() const
{
    return settingsSync ? settingsSync->GetCurrentWorkshopPath() : "";
}

// #detailed comments: UpdateTrainingPackList
// Purpose: Launches an external PowerShell script to download the latest
// training pack data and write a JSON cache to disk. This is intentionally
// performed in a background task to avoid any blocking on the UI/game thread.
//
// Safety and behavior notes:
//  - scrapingInProgress is a guard flag ensuring only one update
//    runs at a time. It is set before launching and cleared when the
//    background process finishes.
//  - The implementation uses system() and relies on the platform's
//    default process creation semantics; this must remain as-is for
//    portability with existing deployments. If this is changed to a
//    more advanced process API, ensure identical detach/exit semantics.
//  - The script path is hard-coded to the repo dev path; callers should
//    ensure that the script is present when invoking this routine.
//
// DO NOT CHANGE: Modifying the background thread logic or the way the
// result is checked could resurface race conditions that previously
// required this exact coordination.
void SuiteSpot::UpdateTrainingPackList()
{
    if (trainingPackMgr) {
        trainingPackMgr->UpdateTrainingPackList(GetTrainingPacksPath(), gameWrapper);
    }
}

BAKKESMOD_PLUGIN(SuiteSpot, "SuiteSpot", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

float SuiteSpot::ConvertMenuPercentToDecimal(float menuValue)
{
    if (!std::isfinite(menuValue)) {
        return 1.0f;
    }

    return std::clamp(menuValue, 0.01f, 10.0f);
}

void SuiteSpot::ApplyTrainingGameSpeedFromMenuValue(float menuValue)
{
    officialTrainingGameSpeed = ConvertMenuPercentToDecimal(menuValue);

    if (!IsTrainingGameSpeedFixEnabled()) {
        return;
    }

    CVarWrapper speedCvar = cvarManager->getCvar("sv_soccar_gamespeed");
    if (!speedCvar) {
        return;
    }

    speedCvar.setValue(officialTrainingGameSpeed);
    LOG("Training game speed set to {}", officialTrainingGameSpeed);
}

void SuiteSpot::LoadTrainingGameSpeedHooks()
{
    gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GFxData_Settings_TA.SetTrainingGameSpeed",
                                                   [this](ActorWrapper caller, void* params, std::string eventName) {
                                                       if (params == nullptr) {
                                                           return;
                                                       }

                                                       // SetTrainingGameSpeed params have 8 bytes of padding before the float
                                                       struct SetTrainingGameSpeedParams
                                                       {
                                                           unsigned char _pad[0x8];
                                                           float value;
                                                       };
                                                       float rawValue =
                                                           reinterpret_cast<SetTrainingGameSpeedParams*>(params)->value;

                                                       ApplyTrainingGameSpeedFromMenuValue(rawValue);
                                                   });
}

void SuiteSpot::UnloadTrainingGameSpeedHooks()
{
    gameWrapper->UnhookEvent("Function TAGame.GFxData_Settings_TA.SetTrainingGameSpeed");
}

void SuiteSpot::LoadHooks()
{
    // ===== MATCH EVENT HOOKS =====
    // Re-queue/transition at match end. We use HookEventPost to ensure the game has finished
    // its internal match-end logic before we attempt to load a new map.
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
                               [this](std::string eventName) { GameEndedEvent(eventName); });

    // ===== PACK HEALER - Training Events =====
    // Based on BakkesMod SDK reference documentation

    // Hook: Training pack loaded or restarted
    // Note: IsInCustomTraining() will not yet return true at this point
    gameWrapper->HookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnInit", [this](std::string eventName) {
        if (!IsEnabled()) return;
        LOG("Hook triggered: GameEvent_TrainingEditor_TA.OnInit");
        gameWrapper->SetTimeout([this](GameWrapper* gw) { TryHealCurrentPack(gw); }, 1.5f);
    });

    // Manual heal command
    cvarManager->registerNotifier(
        "ss_heal_current_pack", [this](std::vector<std::string> args) { TryHealCurrentPack(gameWrapper.get()); },
        "Manually heal the currently loaded training pack", PERMISSION_ALL);

    // Hotkey actions handled entirely in HandleKeyPress hook — no setBind/notifiers.
    // Dual-key strictly required: key1 = trigger, key2 = must be held. Both must be non-empty.
    gameWrapper->HookEventWithCaller<
        ActorWrapper>("Function TAGame.GameViewportClient_TA.HandleKeyPress", [this](ActorWrapper caller, void* params,
                                                                                     std::string eventName) {
        auto p = static_cast<HandleKeyPressParams*>(params);
        std::string keyName = gameWrapper->GetFNameByIndex(p->KeyIndex);
        if (keyName.empty() || keyName == "None") return;

        // Maintain heldKeys set (press = insert, release = erase).
        if (p->EventType == 0) // IE_Pressed
            heldKeys.insert(keyName);
        else if (p->EventType == 1) // IE_Released
            heldKeys.erase(keyName);

        // Fire hotkey actions on press only. Both keys must be non-empty (dual-key enforced).
        if (p->EventType == 0 && settingsSync && mapManager) {
            // Returns true if key1 matches the pressed key and key2 is currently held.
            auto check = [&](const std::string& k1, const std::string& k2) -> bool {
                return !k1.empty() && !k2.empty() && keyName == k1 && heldKeys.count(k2) > 0;
            };

            if (check(settingsSync->GetHotkeyMapModeFwdKey1(), settingsSync->GetHotkeyMapModeFwdKey2())) {
                LOG("Hotkey: cycle_map_mode_fwd");
                mapManager->CycleMapMode(true);
                int newMode = mapManager->GetCurrentMapModeIndex();
                static constexpr const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
                gameWrapper->Toast("", std::string("Map Mode: ") + modeNames[newMode], "default", 3.5f, ToastType_Info);
                cvarManager->getCvar("suitespot_map_type").setValue(newMode);
            } else if (check(settingsSync->GetHotkeyMapModeBkKey1(), settingsSync->GetHotkeyMapModeBkKey2())) {
                LOG("Hotkey: cycle_map_mode_bk");
                mapManager->CycleMapMode(false);
                int newMode = mapManager->GetCurrentMapModeIndex();
                static constexpr const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
                gameWrapper->Toast("", std::string("Map Mode: ") + modeNames[newMode], "default", 3.5f, ToastType_Info);
                cvarManager->getCvar("suitespot_map_type").setValue(newMode);
            } else if (check(settingsSync->GetHotkeyCycleMapFwdKey1(), settingsSync->GetHotkeyCycleMapFwdKey2())) {
                LOG("Hotkey: cycle_map_fwd");
                mapManager->CycleMap(true);
                int mode = mapManager->GetCurrentMapModeIndex();
                std::string mapName;
                if (mode == 0) {
                    mapName = mapManager->GetCurrentFreeplayName();
                    cvarManager->getCvar("suitespot_current_freeplay_code").setValue(mapManager->GetCurrentFreeplayCode());
                } else if (mode == 1) {
                    mapName = mapManager->GetCurrentTrainingName();
                    cvarManager->getCvar("suitespot_current_training_code").setValue(mapManager->GetCurrentTrainingCode());
                } else if (mode == 2) {
                    mapName = mapManager->GetCurrentWorkshopName();
                    cvarManager->getCvar("suitespot_current_workshop_path").setValue(mapManager->GetCurrentWorkshopPath());
                }
                gameWrapper->Toast("", "Map: " + mapName, "default", 3.5f, ToastType_Info);
            } else if (check(settingsSync->GetHotkeyCycleMapBkKey1(), settingsSync->GetHotkeyCycleMapBkKey2())) {
                LOG("Hotkey: cycle_map_bk");
                mapManager->CycleMap(false);
                int mode = mapManager->GetCurrentMapModeIndex();
                std::string mapName;
                if (mode == 0) {
                    mapName = mapManager->GetCurrentFreeplayName();
                    cvarManager->getCvar("suitespot_current_freeplay_code").setValue(mapManager->GetCurrentFreeplayCode());
                } else if (mode == 1) {
                    mapName = mapManager->GetCurrentTrainingName();
                    cvarManager->getCvar("suitespot_current_training_code").setValue(mapManager->GetCurrentTrainingCode());
                } else if (mode == 2) {
                    mapName = mapManager->GetCurrentWorkshopName();
                    cvarManager->getCvar("suitespot_current_workshop_path").setValue(mapManager->GetCurrentWorkshopPath());
                }
                gameWrapper->Toast("", "Map: " + mapName, "default", 3.5f, ToastType_Info);
            } else if (check(settingsSync->GetHotkeyLoadNowKey1(), settingsSync->GetHotkeyLoadNowKey2())) {
                LOG("Hotkey: load_now");
                int mapType = settingsSync->GetMapType();
                std::string cmd;
                if (mapType == 0) {
                    auto code = settingsSync->GetCurrentFreeplayCode();
                    if (!code.empty()) cmd = "load_freeplay " + code;
                } else if (mapType == 1) {
                    auto code = settingsSync->GetCurrentTrainingCode();
                    if (!code.empty()) cmd = "load_training " + code;
                } else if (mapType == 2) {
                    auto path = settingsSync->GetCurrentWorkshopPath();
                    if (!path.empty()) cmd = "load_workshop \"" + path + "\"";
                }
                if (!cmd.empty()) {
                    gameWrapper->Execute([this, cmd](GameWrapper*) { cvarManager->executeCommand(cmd); });
                } else {
                    gameWrapper->Toast("", "No map selected", "default", 3.5f, ToastType_Error);
                }
            }
        }

        // Hotkey capture UI — only active when captureRow >= 0.
        if (captureRow < 0) return;
        if (p->EventType != 0) return; // capture on press only

        LOG("Hotkey Capture: Detected key {} (Index: {}, Gamepad: {})", keyName, p->KeyIndex, p->bGamepad);

        if (keyName == "Escape") {
            LOG("Hotkey Capture: Cancelled via Escape");
            captureRow = -1;
            return;
        }

        if (captureRow < 5) {
            const auto& row = UI::SettingsUI::HOTKEY_ROWS[captureRow];
            const char* cvarName = (captureSlot == 0) ? row.key1CVar : row.key2CVar;
            LOG("Hotkey Capture: Assigning {} to {}", keyName, cvarName);
            gameWrapper->Execute([this, cvarName, keyName](GameWrapper* gw) {
                UI::Helpers::SetCVarSafely(cvarName, keyName, cvarManager, gameWrapper);
            });
            captureRow = -1;
        }
    });
}

// #detailed comments: GameEndedEvent
// Purpose: Called by hooked game events when a match ends. The function
// runs the auto-load logic if enabled.
//
// Timing and ordering notes:
//  - The postMatch.start timestamp is recorded with steady_clock so
//    overlay lifetime calculations are not affected by system clock
//    adjustments.
//
// DO NOT CHANGE: The safeExecute lambda intentionally accepts a delay
// (in seconds) and either executes immediately or schedules via
// gameWrapper->SetTimeout. Changing its semantics will alter when
// external commands (load_freeplay, queue, etc.) are run relative to
// overlay presentation.
void SuiteSpot::GameEndedEvent(std::string name)
{
    if (!IsEnabled()) return;

    LOG("SuiteSpot: GameEndedEvent triggered by hook: {}", name);

    // 1. Run Auto-Load/Queue Logic first (Independent of overlay)
    if (autoLoadFeature && settingsSync) {
        LOG("SuiteSpot: Triggering AutoLoadFeature::OnMatchEnded");

        autoLoadFeature->OnMatchEnded(gameWrapper, cvarManager, SuiteMaps, SuiteTraining, SuiteWorkshop, *settingsSync,
                                      usageTracker.get());

        // Usage tracking handled by AutoLoadFeature::OnMatchEnded
    }
}

// Helper method to extract and heal pack data from current training session
void SuiteSpot::TryHealCurrentPack(GameWrapper* gw)
{
    if (!trainingPackMgr) {
        LOG("SuiteSpot: TryHealCurrentPack - trainingPackMgr is null");
        return;
    }

    if (!gw) {
        LOG("SuiteSpot: TryHealCurrentPack - GameWrapper is null");
        return;
    }

    if (!gw->IsInCustomTraining()) {
        LOG("SuiteSpot: TryHealCurrentPack - Not in custom training (IsInCustomTraining=false)");
        return;
    }

    LOG("SuiteSpot: TryHealCurrentPack - In custom training, attempting to get data...");

    auto server = gw->GetGameEventAsServer();
    if (!server) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to get GameEventAsServer");
        return;
    }

    TrainingEditorWrapper editor(server.memory_address);
    if (!editor) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to create TrainingEditorWrapper");
        return;
    }

    auto trainingData = editor.GetTrainingData();
    if (!trainingData) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to get TrainingData");
        return;
    }

    auto saveData = trainingData.GetTrainingData();
    if (!saveData) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to get TrainingEditorSaveData");
        return;
    }

    std::string code = saveData.GetCode().ToString();

    if (code.empty()) {
        LOG("SuiteSpot: TryHealCurrentPack - Pack code is empty");
        return;
    }

    // Try multiple methods to get shot count
    int realShots = 0;

    // Method 1: From TrainingEditorWrapper directly
    realShots = editor.GetTotalRounds();
    LOG("SuiteSpot: Method 1 (editor.GetTotalRounds): {}", realShots);

    // Method 2: From save data (backup)
    if (realShots <= 0) {
        realShots = saveData.GetNumRounds();
        LOG("SuiteSpot: Method 2 (saveData.GetNumRounds): {}", realShots);
    }

    if (realShots <= 0) {
        LOG("SuiteSpot: [ERR] All methods failed to extract shot count (got {})", realShots);
        return;
    }

    LOG("SuiteSpot: [OK] Successfully extracted pack data - Code: {}, Shots: {}", code, realShots);
    LOG("SuiteSpot: Calling HealPack...");
    trainingPackMgr->HealPack(code, realShots);
}

void SuiteSpot::onLoad()
{
    _globalCvarManager = cvarManager;
    LOG("SuiteSpot loaded");
    mapManager = std::make_unique<MapManager>();
    settingsSync = std::make_unique<SettingsSync>();
    autoLoadFeature = std::make_unique<AutoLoadFeature>();
    trainingPackMgr = std::make_unique<TrainingPackManager>();
    settingsUI = std::make_unique<SettingsUI>(this);
    trainingPackUI = std::make_shared<TrainingPackUI>(this);
    loadoutUI = std::make_unique<LoadoutUI>(this);

    EnsureDataDirectories();
    LoadWorkshopMaps();

    // Initialize LoadoutManager
    loadoutManager = std::make_unique<LoadoutManager>(gameWrapper);
    LOG("SuiteSpot: LoadoutManager initialized");

    // Initialize PackUsageTracker
    usageTracker = std::make_unique<PackUsageTracker>(GetSuiteTrainingDir() / "pack_usage_stats.json");
    LOG("SuiteSpot: PackUsageTracker initialized");

    // Initialize WorkshopDownloader
    workshopDownloader = std::make_shared<WorkshopDownloader>(gameWrapper);
    LOG("SuiteSpot: WorkshopDownloader initialized");

    // Initialize TextureDownloader
    textureDownloader = std::make_unique<TextureDownloader>(gameWrapper, cvarManager);
    LOG("SuiteSpot: TextureDownloader initialized");

    // Check Pack cache and load if available

    if (trainingPackMgr) {
        if (!std::filesystem::exists(GetTrainingPacksPath())) {
            LOG("SuiteSpot: No Pack cache found. Schedule scraping on next opportunity.");
            // Will be scraped on first Settings render or user request
        } else {
            // Load existing Pack cache
            trainingPackMgr->LoadPacksFromFile(GetTrainingPacksPath());
            LOG("SuiteSpot: Pack cache loaded");
        }
    }

    LoadHooks();

    if (settingsSync) {
        settingsSync->RegisterAllCVars(cvarManager);
        // Restore saved CVar values (hotkeys, etc.) that may have been reset by re-registration on hot-reload.
        cvarManager->loadCfg("config");

        // Always check workshop textures on launch — required for workshop feature to work
        if (textureDownloader) {
            LOG("SuiteSpot: Checking workshop textures...");
            std::vector<std::string> missing = textureDownloader->CheckMissingTextures();
            if (missing.empty()) {
                LOG("SuiteSpot: All {} workshop textures present.", textureDownloader->WorkshopTexturesFilesList.size());
            } else {
                LOG("SuiteSpot: {} missing texture(s) detected. Auto-downloading...", missing.size());
                if (textureDownloadThread.joinable()) {
                    textureDownloadThread.join();
                }
                textureDownloadThread = std::thread([this]() { textureDownloader->DownloadAndInstallTextures(); });
            }
        }
    }
    LoadTrainingGameSpeedHooks();

    LOG("SuiteSpot: Plugin initialization complete");
}

// #detailed comments: onUnload
// Purpose: Clean up all SDK resources in proper order to enable hot-reload.
//
// Cleanup sequence (CRITICAL - must follow this order):
//  1. Join all background threads (prevents dangling thread callbacks)
//  2. Save pending data (ensures no data loss)
//  3. Unhook events (removes game event callbacks pointing to freed memory)
//  4. Reset UI components (releases ImGui resources)
//  5. Reset managers (releases business logic)
//  6. Clear ImGui context
//
// Note: BakkesMod automatically cleans up CVars and notifiers on plugin unload,
// but event hooks MUST be manually unhooked to prevent hot-reload crashes.
//
// DO NOT CHANGE: This sequence prevents hot-reload crashes by ensuring
// all callbacks are removed before the plugin DLL is unloaded. Skipping
// event unhooking will cause BakkesMod to call freed memory on reload.
void SuiteSpot::onUnload()
{
    LOG("SuiteSpot unloading...");

    // Wait for texture download to complete if running
    if (textureDownloadThread.joinable()) {
        LOG("SuiteSpot: Waiting for texture download to complete...");
        textureDownloadThread.join();
    }

    // Stop workshop downloader search thread
    if (workshopDownloader) {
        workshopDownloader->StopSearch();
    }

    if (usageTracker) {
        usageTracker->SaveStats();
    }

    UnloadTrainingGameSpeedHooks();

    // STEP 3: Unhook all game events (CRITICAL - SDK requirement)
    gameWrapper->UnhookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded");
    gameWrapper->UnhookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnInit");
    gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleKeyPress");
    LOG("Event hooks removed");

    // STEP 4: Reset UI components (releases ImGui resources)
    settingsUI.reset();
    trainingPackUI.reset();
    loadoutUI.reset();
    LOG("UI components destroyed");

    // STEP 5: Reset data managers
    trainingPackMgr.reset();
    autoLoadFeature.reset();
    settingsSync.reset();
    mapManager.reset();
    workshopDownloader.reset();
    LOG("Managers destroyed");

    // STEP 6: Clear ImGui context
    imgui_ctx = 0;

    LOG("SuiteSpot unloaded successfully");
}

void SuiteSpot::Render()
{
    if (!imgui_ctx) return;
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(imgui_ctx));

    // Note: TrainingPackUI is a PluginWindow registered with BakkesMod,
    // so it's rendered automatically by the framework. No need to call it here.
}

std::string SuiteSpot::GetMenuName()
{

    return "suitespot_browser";
}

std::string SuiteSpot::GetMenuTitle()
{

    return "SuiteSpot Training Browser";
}

void SuiteSpot::SetImGuiContext(uintptr_t ctx)
{

    if (ctx) {

        imgui_ctx = ctx;

        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));

        // Load clock font — GetFont first (hot-reload: font already in atlas, no rebuild)
        // Falls back to Execute + LoadFont (cold start: defers atlas rebuild to game thread)
        // Guard inside lambda: Execute may be queued multiple times if SetImGuiContext fires
        // before the first callback runs; the inner check prevents duplicate atlas rebuilds.
        if (!clockFont) {
            auto gui = gameWrapper->GetGUIManager();
            clockFont = gui.GetFont("suitespot_clock_48");
            if (!clockFont) {
                gameWrapper->Execute([this](GameWrapper* gw) {
                    if (clockFont) return; // already loaded by an earlier Execute
                    auto gui = gw->GetGUIManager();
                    auto [res, font] = gui.LoadFont("suitespot_clock_48", "Ubuntu-Regular.ttf", 48);
                    if (res == 2 && font) {
                        clockFont = font;
                    }
                });
            }
        }

        // Load UI font: Roboto-Medium 14px + FA5 Solid icons merged in (glyph range F000-F8D9)
        if (!uiFont) {
            auto gui = gameWrapper->GetGUIManager();
            uiFont = gui.GetFont("suitespot_roboto_14");
            if (!uiFont) {
                gameWrapper->Execute([this](GameWrapper* gw) {
                    if (uiFont) return; // already loaded by an earlier Execute
                    auto gui = gw->GetGUIManager();
                    // Base font: Roboto Medium at 14px
                    auto [r1, roboto] = gui.LoadFont("suitespot_roboto_14", "Roboto-Medium.ttf", 14);
                    if (r1 == 2 && roboto) uiFont = roboto;
                    // Merge FA5 Solid icons into Roboto (must immediately follow base font load)
                    static ImFontConfig fa_cfg;
                    fa_cfg.MergeMode = true;
                    fa_cfg.PixelSnapH = true;
                    fa_cfg.GlyphMinAdvanceX = 13.0f;
                    static const ImWchar fa_ranges[] = {0xf000, 0xf8d9, 0}; // FA5 Solid range
                    gui.LoadFont("suitespot_fa_solid_14", "fa-solid-900.ttf", 14, &fa_cfg, fa_ranges);
                });
            }
        }
    }
}

bool SuiteSpot::ShouldBlockInput()
{
    if (!isBrowserOpen) {
        return false; // Browser closed → no blocking
    }

    // Selective input blocking - consistent with TrainingPackUI
    ImGuiIO& io = ImGui::GetIO();

    // Block when actively typing in text fields (settings UI)
    if (io.WantTextInput && ImGui::IsAnyItemActive()) {
        return true;
    }

    // Allow normal mouse interaction without blocking game input
    return false;
}

bool SuiteSpot::IsActiveOverlay()
{

    return isBrowserOpen;
}

void SuiteSpot::OnOpen()
{

    LOG("SuiteSpot: OnOpen called");

    isBrowserOpen = true;

    if (trainingPackUI) {

        trainingPackUI->SetOpen(true);
    }
}

void SuiteSpot::OnClose()
{
    LOG("SuiteSpot: OnClose called (Ignoring state change to keep browser open)");
}

std::filesystem::path SuiteSpot::GetTrainingPacksPath() const
{
    return mapManager ? mapManager->GetTrainingPacksPath() : std::filesystem::path();
}

void SuiteSpot::LoadTrainingPacksFromFile(const std::filesystem::path& filePath)
{
    if (trainingPackMgr) {
        trainingPackMgr->LoadPacksFromFile(filePath);
    }
}
