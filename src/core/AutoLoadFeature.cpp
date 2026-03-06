#include "pch.h"
#include "AutoLoadFeature.h"
#include "MapList.h"
#include "SettingsSync.h"
#include "DefaultPacks.h"
#include "PackUsageTracker.h"

#include <algorithm>
#include <random>

void AutoLoadFeature::OnMatchEnded(std::shared_ptr<GameWrapper> gameWrapper,
                                   std::shared_ptr<CVarManagerWrapper> cvarManager,
                                   const std::vector<MapEntry>& freeplayMaps,
                                   const std::vector<TrainingEntry>& trainingPacks,
                                   const std::vector<WorkshopEntry>& workshopMaps,
                                   SettingsSync& settings,
                                   PackUsageTracker* usageTracker)
{
    if (!gameWrapper || !cvarManager) return;
    if (!settings.IsEnabled()) return;

    const int mapType = settings.GetMapType();
    const int delayQueueSec = settings.GetDelayQueueSec();
    const int delayFreeplaySec = settings.GetDelayFreeplaySec();
    const int delayTrainingSec = settings.GetDelayTrainingSec();
    const int delayWorkshopSec = settings.GetDelayWorkshopSec();

    std::string currentFreeplayCode = settings.GetCurrentFreeplayCode();
    std::string currentTrainingCode = settings.GetCurrentTrainingCode();
    std::string currentWorkshopPath = settings.GetCurrentWorkshopPath();

    auto safeExecute = [&](int delaySec, const std::string& cmd) {
        // Enforce a minimum delay of 0.1s to ensure the game state has settled after the match.
        // Even if the user sets 0s, we want to force a context switch out of the event stack.
        float actualDelay = (delaySec <= 0) ? 0.1f : static_cast<float>(delaySec);

        gameWrapper->SetTimeout([cvarManager, cmd](GameWrapper* gw) {
            cvarManager->executeCommand(cmd);
        }, actualDelay);
    };

    int mapLoadDelay = 0;

    if (mapType == 0) { // Freeplay
        if (currentFreeplayCode.empty()) {
            LOG("SuiteSpot: ⚠️ No freeplay map selected; skipping load.");
        } else {
            // Verify the map code exists in the list
            auto it = std::find_if(freeplayMaps.begin(), freeplayMaps.end(),
                [&](const MapEntry& e) { return e.code == currentFreeplayCode; });
            if (it != freeplayMaps.end()) {
                safeExecute(delayFreeplaySec, "load_freeplay " + currentFreeplayCode);
                mapLoadDelay = delayFreeplaySec;
                LOG("SuiteSpot: [OK] Loading freeplay map: {}", it->name);
            } else {
                LOG("SuiteSpot: [ERR] Freeplay map '{}' not found. Available maps: {}",
                    currentFreeplayCode, freeplayMaps.size());
            }
        }
    } else if (mapType == 1) { // Training
        std::string codeToLoad;
        std::string nameToLoad;

        // Single Pack Mode: use quick picks selection
        std::string targetCode = settings.GetQuickPicksSelectedCode();
        
        // If empty, try fallback to current training code (legacy)
        if (targetCode.empty()) targetCode = settings.GetCurrentTrainingCode();

        // Resolve target code
        if (!targetCode.empty()) {
            // FIX: Trust BakkesMod to handle invalid codes. Don't validate against cache.
            // This allows loading codes that aren't in our cached list (fresh/shared codes).
            codeToLoad = targetCode;

            // Try to find name in cache for logging, but don't require it
            auto it = std::find_if(trainingPacks.begin(), trainingPacks.end(),
                [&](const TrainingEntry& e) { return e.code == targetCode; });
            if (it != trainingPacks.end()) {
                nameToLoad = it->name;
            } else {
                nameToLoad = targetCode; // Use code as name if not in cache
            }
        }

        // Fallback to first Quick Pick if nothing selected or valid found
        if (codeToLoad.empty()) {
            std::vector<std::string> quickPicks;
            if (usageTracker && !usageTracker->IsFirstRun()) {
                quickPicks = usageTracker->GetTopUsedCodes(settings.GetQuickPicksCount());
            }
            
            if (quickPicks.empty()) {
                for(const auto& p : DefaultPacks::FLICKS_PICKS) quickPicks.push_back(p.code);
            }

            if (!quickPicks.empty()) {
                std::string fallbackCode = quickPicks[0];
                auto it = std::find_if(trainingPacks.begin(), trainingPacks.end(),
                    [&](const TrainingEntry& e) { return e.code == fallbackCode; });
                
                codeToLoad = fallbackCode;
                nameToLoad = (it != trainingPacks.end()) ? it->name : "Quick Pick Fallback";
                LOG("SuiteSpot: Selected pack missing, falling back to first Quick Pick: {}", nameToLoad);
            }
        }

        if (!codeToLoad.empty()) {
            // Increment usage stats for auto-loaded packs
            if (usageTracker) {
                usageTracker->IncrementLoadCount(codeToLoad);
            }

            safeExecute(delayTrainingSec, "load_training " + codeToLoad);
            mapLoadDelay = delayTrainingSec;
            LOG("SuiteSpot: Loading training pack: " + nameToLoad);
        } else {
            LOG("SuiteSpot: No training pack to load.");
        }
    } else if (mapType == 2) { // Workshop

        if (currentWorkshopPath.empty()) {
            LOG("SuiteSpot: ⚠️ No workshop map selected; skipping load.");
        } else {
            // Verify the workshop map exists in the list
            auto it = std::find_if(workshopMaps.begin(), workshopMaps.end(),
                [&](const WorkshopEntry& e) { return e.filePath == currentWorkshopPath; });
            if (it != workshopMaps.end()) {
                safeExecute(delayWorkshopSec, "load_workshop \"" + currentWorkshopPath + "\"");
                mapLoadDelay = delayWorkshopSec;
                LOG("SuiteSpot: [OK] Loading workshop map: {}", it->name);
            } else {
                LOG("SuiteSpot: [ERR] Workshop map not found: {}", currentWorkshopPath);
                LOG("SuiteSpot: 💡 Check WorkshopMapLoader plugin settings for maps folder path");
            }
        }
    }

    if (settings.IsAutoQueue()) {
        safeExecute(delayQueueSec, "queue");
        LOG("SuiteSpot: Auto-Queuing scheduled with delay: " + std::to_string(delayQueueSec) + "s.");
    }
}
