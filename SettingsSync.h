#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <memory>

/*
 * ======================================================================================
 * SETTINGS SYNC: THE CONFIGURATION MANAGER
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class manages all the "CVars" (Console Variables). CVars are how BakkesMod stores
 * settings so they are remembered when you restart the game.
 * 
 * WHY IS IT HERE?
 * We need a single, safe place to read and write settings. If we scattered this logic
 * everywhere, we might misspell a setting name or use the wrong default value.
 * 
 * HOW DOES IT WORK?
 * 1. `RegisterAllCVars()`: Called once at startup. It tells BakkesMod: "Hey, I have a setting called 
 *    'suitespot_enabled', please remember it for me."
 * 2. It keeps a local copy of every setting (e.g., `bool enabled`) for fast access.
 * 3. When BakkesMod says "The user changed this setting in the console," this class 
 *    automatically updates its local copy.
 */

class SettingsSync
{
  public:
    // Tells BakkesMod about all our settings
    void RegisterAllCVars(const std::shared_ptr<CVarManagerWrapper>& cvarManager);

    // Getters: Fast, safe ways to ask "Is this feature on?"
    bool IsEnabled() const { return enabled; }
    int GetMapType() const { return mapType; }
    bool IsAutoQueue() const { return autoQueue; }
    bool IsTrainingGameSpeedFixEnabled() const { return trainingGameSpeedFixEnabled; }
    int GetQuickPicksListType() const { return quickPicksListType; }
    int GetQuickPicksCount() const { return quickPicksCount; }
    std::string GetQuickPicksSelected() const { return quickPicksSelected; }

    // Delay getters (How long to wait?)
    int GetDelayQueueSec() const { return delayQueueSec; }
    int GetDelayFreeplaySec() const { return delayFreeplaySec; }
    int GetDelayTrainingSec() const { return delayTrainingSec; }
    int GetDelayWorkshopSec() const { return delayWorkshopSec; }

    // Texture settings
    bool IsAutoDownloadTextures() const { return autoDownloadTextures; }

    // Selection getters (Which map/pack is selected?)
    std::string GetCurrentFreeplayCode() const { return currentFreeplayCode; }
    std::string GetCurrentTrainingCode() const { return currentTrainingCode; }
    std::string GetQuickPicksSelectedCode() const { return quickPicksSelected; }
    std::string GetCurrentWorkshopPath() const { return currentWorkshopPath; }

    // Hotkey getters — key name (UE3 string, e.g. "J", "F3") + modifier (0=None, 16=Shift, 17=Ctrl, 18=Alt)
    std::string GetHotkeyMapModeFwdKey() const { return hotkeyMapModeFwdKey; }
    int GetHotkeyMapModeFwdMod() const { return hotkeyMapModeFwdMod; }
    std::string GetHotkeyMapModeBkKey() const { return hotkeyMapModeBkKey; }
    int GetHotkeyMapModeBkMod() const { return hotkeyMapModeBkMod; }
    std::string GetHotkeyCycleMapFwdKey() const { return hotkeyCycleMapFwdKey; }
    int GetHotkeyCycleMapFwdMod() const { return hotkeyCycleMapFwdMod; }
    std::string GetHotkeyCycleMapBkKey() const { return hotkeyCycleMapBkKey; }
    int GetHotkeyCycleMapBkMod() const { return hotkeyCycleMapBkMod; }
    std::string GetHotkeyLoadNowKey() const { return hotkeyLoadNowKey; }
    int GetHotkeyLoadNowMod() const { return hotkeyLoadNowMod; }

    // Setters: Update the local value (used when loading data)
    void SetCurrentFreeplayCode(const std::string& code);
    void SetCurrentTrainingCode(const std::string& code);
    void SetQuickPicksSelected(const std::string& code);
    void SetCurrentWorkshopPath(const std::string& path);

  private:
    // Local copies of settings for fast access
    bool enabled = false;
    int mapType = 0; // 0=Freeplay, 1=Training, 2=Workshop
    bool autoQueue = false;
    bool trainingGameSpeedFixEnabled = true;
    int quickPicksListType = 0; // 0=Flicks Picks, 1=Your Favorites
    int quickPicksCount = 10;
    std::string quickPicksSelected = "";

    int delayQueueSec = 0;
    int delayFreeplaySec = 0;
    int delayTrainingSec = 0;
    int delayWorkshopSec = 0;

    bool autoDownloadTextures = false;

    std::string currentFreeplayCode; // Freeplay map code (e.g., "beckwith_park_p")
    std::string currentTrainingCode; // Training pack code (e.g., "XXXX-XXXX-XXXX-XXXX")
    std::string currentWorkshopPath; // Workshop map path (e.g., "C:/path/to/map.udk")

    // Hotkey bindings: key name (UE3 string) + modifier code (0=None, 16=Shift, 17=Ctrl, 18=Alt)
    std::string hotkeyMapModeFwdKey;
    int hotkeyMapModeFwdMod = 0;
    std::string hotkeyMapModeBkKey;
    int hotkeyMapModeBkMod = 0;
    std::string hotkeyCycleMapFwdKey;
    int hotkeyCycleMapFwdMod = 0;
    std::string hotkeyCycleMapBkKey;
    int hotkeyCycleMapBkMod = 0;
    std::string hotkeyLoadNowKey;
    int hotkeyLoadNowMod = 0;
};
