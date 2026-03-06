#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>

/*
 * ======================================================================================
 * LOADOUT MANAGER: THE CAR CHANGER
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class handles changing your car preset (e.g., switching from "Octane" to "Fennec").
 * 
 * WHY IS IT HERE?
 * BakkesMod's loadout API is a bit complex and runs on the Game Thread.
 * This manager wraps all that complexity so the UI can just say "Switch to Fennec" 
 * without worrying about crashes or thread safety.
 * 
 * HOW DOES IT WORK?
 * 1. It asks the game for a list of all your presets ("Octane", "Dominus", etc.).
 * 2. It saves this list safely.
 * 3. When you click "Apply Loadout", it tells the game to equip that preset immediately.
 * 4. It uses `gameWrapper->Execute` to make sure we don't crash by touching game data
 *    from the wrong thread.
 */

class LoadoutManager
{
public:
    explicit LoadoutManager(std::shared_ptr<GameWrapper> gameWrapper);
    
    std::vector<std::string> GetLoadoutNames();
    
    void GetCurrentLoadoutName(std::function<void(std::string)> onComplete);
    
    void SwitchLoadout(const std::string& loadoutName, std::function<void(bool)> onComplete = nullptr);
    
    void SwitchLoadout(int index, std::function<void(bool)> onComplete = nullptr);
    
    bool RefreshLoadoutCache();
    
    bool IsReady() const;

private:
    std::shared_ptr<GameWrapper> gameWrapper_;
    
    std::vector<std::string> cachedLoadoutNames_;
    
    mutable std::mutex cacheMutex_;
    std::atomic<bool> initialized_{false};
    
    void QueryLoadoutNamesInternal(std::function<void(size_t)> onComplete = nullptr);
};
