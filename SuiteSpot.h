#pragma once

/*
 * ======================================================================================
 * SUITESPOT: PLUGIN ARCHITECTURE & MAIN HUB
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This is the "Brain" of the SuiteSpot plugin. The `SuiteSpot` class is the central
 * hub that connects BakkesMod (the game) to all our custom features.
 * 
 * WHY IS IT HERE?
 * BakkesMod requires one main class that inherits from `BakkesModPlugin` to start
 * everything up. This file tells the game "I am a plugin, here is how to load me."
 * 
 * HOW DOES IT WORK?
 * 1. LIFECYCLE:
 *    - `onLoad()`: Called when Rocket League starts. We create all our tools (Managers, UI) here.
 *    - `onUnload()`: Called when the game closes. We clean up memory here.
 * 
 * 2. THE BRIDGE:
 *    - It holds pointers to "Managers" (MapManager, TrainingPackManager) which handle data.
 *    - It holds pointers to "UI" (SettingsUI, TrainingPackUI) which draw the menus.
 *    - It listens for game events (like "Match Ended") and tells the `AutoLoadFeature` to react.
 * 
 * 3. WINDOW MANAGEMENT (Advanced):
 *    - This class is special because it acts as BOTH the "Settings Tab" (inside F2 menu)
 *      AND the "Window Manager" for our pop-up browser.
 *    - It uses a "Hybrid Rendering" trick to keep the browser window open even when F2 is closed.
 *      (See `docs/development/thread-safe-imgui.md` for the technical details).
 */

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "MapList.h"
#include "LoadoutManager.h"
#include "PackUsageTracker.h"
#include "TextureDownloader.h"
#include "StatusMessageUI.h"
#include "version.h"
#include <filesystem>
#include <set>
#include <memory>

#include <atomic>

class SettingsWindowBase : public BakkesMod::Plugin::PluginSettingsWindow
{
  public:
    virtual ~SettingsWindowBase() = default;
    std::string GetPluginName() override { return "SuiteSpot"; }
    void SetImGuiContext(uintptr_t ctx) override { ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx)); }
};

// Forward declarations
class MapManager;
class SettingsSync;
class AutoLoadFeature;
class TrainingPackManager;
class WorkshopDownloader;
class SettingsUI;
class TrainingPackUI;
class LoadoutUI;

// Version macro carried over from the master template
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(
    VERSION_PATCH) "." stringify(VERSION_BUILD);

class SuiteSpot final : public BakkesMod::Plugin::BakkesModPlugin,
                        public SettingsWindowBase,
                        public BakkesMod::Plugin::PluginWindow
{
    friend class SettingsUI;
    friend class TrainingPackUI;
    friend class LoadoutUI;

  public:
    // PluginWindow implementation
    void Render() override;
    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    void SetImGuiContext(uintptr_t ctx) override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    void OnOpen() override;
    void OnClose() override;
    // Persistence folders and files under %APPDATA%\bakkesmod\bakkesmod\data
    void EnsureDataDirectories() const;
    std::filesystem::path GetDataRoot() const;
    std::filesystem::path GetSuiteTrainingDir() const;

    // Workshop persistence API
    void LoadWorkshopMaps();
    void DiscoverWorkshopInDir(const std::filesystem::path& dir);
    std::filesystem::path GetWorkshopLoaderConfigPath() const;
    std::filesystem::path ResolveConfiguredWorkshopRoot() const;

    // lifecycle
    void onLoad() override;
    void onUnload() override;

    // settings UI (PluginSettingsWindow)
    void RenderSettings() override;

    // hooks
    void LoadHooks();
    void GameEndedEvent(std::string name);
    void TryHealCurrentPack(GameWrapper* gw); // Pack healer helper

    // Training Pack update integration
    std::filesystem::path GetTrainingPacksPath() const;
    void UpdateTrainingPackList();
    void LoadTrainingPacksFromFile(const std::filesystem::path& filePath);
    bool IsTrainingPackCacheStale() const;
    std::string FormatLastUpdatedTime() const;

    bool IsEnabled() const;
    bool IsAutoQueueEnabled() const;
    bool IsTrainingGameSpeedFixEnabled() const;
    int GetMapType() const;
    int GetDelayQueueSec() const;
    int GetDelayFreeplaySec() const;
    int GetDelayTrainingSec() const;
    int GetDelayWorkshopSec() const;
    std::string GetCurrentFreeplayCode() const;
    std::string GetCurrentTrainingCode() const;
    std::string GetCurrentWorkshopPath() const;

  private:
    void LoadTrainingGameSpeedHooks();
    void UnloadTrainingGameSpeedHooks();
    void ApplyTrainingGameSpeedFromMenuValue(float menuValue);
    static float ConvertMenuPercentToDecimal(float menuValue);

    // Loadout management
    std::unique_ptr<LoadoutManager> loadoutManager;
    std::unique_ptr<PackUsageTracker> usageTracker;
    std::shared_ptr<WorkshopDownloader> workshopDownloader;
    std::unique_ptr<TextureDownloader> textureDownloader;

    std::unique_ptr<MapManager> mapManager;
    std::unique_ptr<SettingsSync> settingsSync;
    std::unique_ptr<AutoLoadFeature> autoLoadFeature;
    std::unique_ptr<TrainingPackManager> trainingPackMgr;
    std::unique_ptr<SettingsUI> settingsUI;
    std::shared_ptr<TrainingPackUI> trainingPackUI;
    std::unique_ptr<LoadoutUI> loadoutUI;

    bool isBrowserOpen = false;
    float officialTrainingGameSpeed = 1.0f;
    uintptr_t imgui_ctx = 0;
    ImFont* clockFont = nullptr;
    std::atomic<bool> isRenderingSettings{false};
    std::thread textureDownloadThread; // Managed texture download thread

    // Toast notification system for hotkey feedback
    UI::StatusMessage hotKeyToast;

    // Hotkey capture state
    int captureRow = -1;
    int captureSlot = 0;

    // Tracks currently-held key names (UE3 strings, e.g. "XboxTypeS_X") via HandleKeyPress hook.
    // Updated on every press/release event — never polled. Used for combo key checks in notifiers.
    std::set<std::string> heldKeys;

    // Hotkey handlers
    void ShowToastForAction(const std::string& actionName);
};

struct HandleKeyPressParams
{
    int ControllerId;
    int KeyIndex;
    int KeyNumber;
    unsigned char EventType;
    unsigned char Padding[3];
    float AmountDepressed;
    unsigned int bGamepad;
};
