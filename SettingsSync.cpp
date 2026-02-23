#include "pch.h"
#include "SettingsSync.h"

#include <algorithm>

void SettingsSync::RegisterAllCVars(const std::shared_ptr<CVarManagerWrapper>& cvarManager)
{
    if (!cvarManager) return;

    cvarManager->registerCvar("suitespot_enabled", "0", "Enable SuiteSpot", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { enabled = cvar.getBoolValue(); });

    cvarManager
        ->registerCvar("suitespot_map_type", "0", "Map type: 0=Freeplay, 1=Training, 2=Workshop", true, true, 0, true, 2)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { mapType = cvar.getIntValue(); });

    cvarManager->registerCvar("suitespot_auto_queue", "0", "Enable auto-queuing after map load", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { autoQueue = cvar.getBoolValue(); });

    cvarManager
        ->registerCvar("suitespot_fix_training_gamespeed", "1",
                       "Keep in-game training speed synced with BM in training playlists", true, true, 0, true, 1)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { trainingGameSpeedFixEnabled = cvar.getBoolValue(); });

    cvarManager
        ->registerCvar("suitespot_quickpicks_list_type", "0", "List type: 0=Flicks Picks, 1=Your Favorites", true, true,
                       0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { quickPicksListType = cvar.getIntValue(); });

    cvarManager
        ->registerCvar("suitespot_quickpicks_count", "10", "Number of quick picks to show", true, true, 5, true, 15)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { quickPicksCount = cvar.getIntValue(); });

    cvarManager->registerCvar("suitespot_quickpicks_selected", "", "Selected quick pick pack code", true)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { quickPicksSelected = cvar.getStringValue(); });

    cvarManager
        ->registerCvar("suitespot_delay_queue_sec", "0", "Delay before queuing (seconds)", true, true, 0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayQueueSec = std::max(0, cvar.getIntValue()); });

    cvarManager
        ->registerCvar("suitespot_delay_freeplay_sec", "0", "Delay before loading freeplay map (seconds)", true, true,
                       0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayFreeplaySec = std::max(0, cvar.getIntValue()); });

    cvarManager
        ->registerCvar("suitespot_delay_training_sec", "0", "Delay before loading training map (seconds)", true, true,
                       0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayTrainingSec = std::max(0, cvar.getIntValue()); });

    cvarManager
        ->registerCvar("suitespot_delay_workshop_sec", "0", "Delay before loading workshop map (seconds)", true, true,
                       0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayWorkshopSec = std::max(0, cvar.getIntValue()); });

    cvarManager->registerCvar("suitespot_current_freeplay_code", "", "Currently selected freeplay map code", true)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { currentFreeplayCode = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_current_training_code", "", "Currently selected training pack code", true)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { currentTrainingCode = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_current_workshop_path", "", "Currently selected workshop map path", true)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { currentWorkshopPath = cvar.getStringValue(); });

    cvarManager
        ->registerCvar("suitespot_auto_download_textures", "0", "Auto-download missing workshop textures on launch",
                       true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { autoDownloadTextures = cvar.getBoolValue(); });

    cvarManager->registerCvar("ss_training_maps", "", "Stored training maps", true, false, 0, false, 0);

    // Hotkey bindings: key1 triggers via setBind (event-driven); key2 is the optional held combo partner.
    // Both are free-form UE3 key name strings (e.g. "J", "F3", "LeftAlt", "XboxTypeS_DPad_Up").

    cvarManager->registerCvar("suitespot_hotkey_map_mode_fwd_key", "", "Key 1 for cycle map mode forward", true)
        .addOnValueChanged([this, cvarManager](std::string oldValue, CVarWrapper cvar) {
            if (!oldValue.empty()) cvarManager->setBind(oldValue, "");
            hotkeyMapModeFwdKey = cvar.getStringValue();
            if (!hotkeyMapModeFwdKey.empty()) cvarManager->setBind(hotkeyMapModeFwdKey, "ss_cycle_map_mode_fwd");
        });
    cvarManager->registerCvar("suitespot_hotkey_map_mode_fwd_key2", "", "Key 2 (held) for cycle map mode forward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyMapModeFwdKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_map_mode_bk_key", "", "Key 1 for cycle map mode backward", true)
        .addOnValueChanged([this, cvarManager](std::string oldValue, CVarWrapper cvar) {
            if (!oldValue.empty()) cvarManager->setBind(oldValue, "");
            hotkeyMapModeBkKey = cvar.getStringValue();
            if (!hotkeyMapModeBkKey.empty()) cvarManager->setBind(hotkeyMapModeBkKey, "ss_cycle_map_mode_bk");
        });
    cvarManager->registerCvar("suitespot_hotkey_map_mode_bk_key2", "", "Key 2 (held) for cycle map mode backward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyMapModeBkKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_cycle_map_fwd_key", "", "Key 1 for cycle map forward", true)
        .addOnValueChanged([this, cvarManager](std::string oldValue, CVarWrapper cvar) {
            if (!oldValue.empty()) cvarManager->setBind(oldValue, "");
            hotkeyCycleMapFwdKey = cvar.getStringValue();
            if (!hotkeyCycleMapFwdKey.empty()) cvarManager->setBind(hotkeyCycleMapFwdKey, "ss_cycle_map_fwd");
        });
    cvarManager->registerCvar("suitespot_hotkey_cycle_map_fwd_key2", "", "Key 2 (held) for cycle map forward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyCycleMapFwdKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_cycle_map_bk_key", "", "Key 1 for cycle map backward", true)
        .addOnValueChanged([this, cvarManager](std::string oldValue, CVarWrapper cvar) {
            if (!oldValue.empty()) cvarManager->setBind(oldValue, "");
            hotkeyCycleMapBkKey = cvar.getStringValue();
            if (!hotkeyCycleMapBkKey.empty()) cvarManager->setBind(hotkeyCycleMapBkKey, "ss_cycle_map_bk");
        });
    cvarManager->registerCvar("suitespot_hotkey_cycle_map_bk_key2", "", "Key 2 (held) for cycle map backward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyCycleMapBkKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_load_now_key", "", "Key 1 for load current map immediately", true)
        .addOnValueChanged([this, cvarManager](std::string oldValue, CVarWrapper cvar) {
            if (!oldValue.empty()) cvarManager->setBind(oldValue, "");
            hotkeyLoadNowKey = cvar.getStringValue();
            if (!hotkeyLoadNowKey.empty()) cvarManager->setBind(hotkeyLoadNowKey, "ss_load_now");
        });
    cvarManager->registerCvar("suitespot_hotkey_load_now_key2", "", "Key 2 (held) for load now", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyLoadNowKey2 = cvar.getStringValue(); });

    // Note: CVars auto-initialize to defaults from registerCvar() above
    // The addOnValueChanged callbacks will sync values if user has saved config
    // No need to redundantly set values here
}

void SettingsSync::SetCurrentFreeplayCode(const std::string& code)
{
    currentFreeplayCode = code;
}

void SettingsSync::SetCurrentTrainingCode(const std::string& code)
{
    currentTrainingCode = code;
}

void SettingsSync::SetQuickPicksSelected(const std::string& code)
{
    quickPicksSelected = code;
}

void SettingsSync::SetCurrentWorkshopPath(const std::string& path)
{
    currentWorkshopPath = path;
}
