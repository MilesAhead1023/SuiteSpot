#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "MapList.h"
#include "SettingsSync.h"
#include "logging.h"
#include <string>
#include <vector>
#include <memory>

/*
 * ======================================================================================
 * AUTO LOAD FEATURE: THE AUTOMATION ENGINE
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This is the component that actually "does the work" when a match ends. It is the
 * logic engine of SuiteSpot.
 * 
 * WHY IS IT HERE?
 * We need a dedicated place to decide "What happens next?" after a game.
 * Should we queue? Should we load a map? Which map? This class answers those questions.
 * 
 * HOW DOES IT WORK?
 * 1. `OnMatchEnded(...)`: This function is called by `SuiteSpot` the moment a match finishes.
 * 2. It checks `SettingsSync` to see what the user wants (e.g., "Auto Queue: ON", "Map: Workshop").
 * 3. It calculates delays (e.g., "Wait 5 seconds").
 * 4. It schedules commands using `gameWrapper->SetTimeout`.
 *    - Example: "In 5 seconds, execute command 'load_workshop my_map.upk'"
 */

class PackUsageTracker;

class AutoLoadFeature
{
public:
    // The main entry point. Called when the match ends.
    // It takes ALL the necessary data (settings, map lists) and decides what to do.
    void OnMatchEnded(std::shared_ptr<GameWrapper> gameWrapper,
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        const std::vector<MapEntry>& freeplayMaps,
        const std::vector<TrainingEntry>& trainingPacks,
        const std::vector<WorkshopEntry>& workshopMaps,
        SettingsSync& settings,
        PackUsageTracker* usageTracker);
};
