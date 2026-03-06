#include "pch.h"
#include "SuiteSpot.h"
#include "SettingsUI.h"
#include "TrainingPackUI.h"

// #detailed comments: RenderSettings
// Purpose: Build the Settings UI using ImGui. This method is called on
// the UI thread and must complete quickly — avoid heavy computation
// or blocking calls here. Instead, schedule background work with
// gameWrapper->SetTimeout if needed.
//
// UI & state invariants:
//  - Any changes to cvars must keep the CVar<->member variable sync
//    consistent. The code uses cvarManager to persist values and also
//    updates plugin members; changing the order of writes may create
//    transient inconsistencies visible to users.
//  - Many UI controls guard against empty lists (e.g., RLTraining) and
//    clamp indices to valid ranges to prevent out-of-bounds access.
//
// DO NOT CHANGE: Rendering flow, label strings, and cvar names are
// relied upon by external automation and saved settings; altering them
// will change user-visible state persistence and CLI integrations.
void SuiteSpot::RenderSettings()
{
    // 1. Render the main settings UI
    if (settingsUI) {
        settingsUI->RenderMainSettingsWindow();
    }

    // 2. Render browser if open (Since RegisterDrawable is blocked)
    if (isBrowserOpen && trainingPackUI) {
        trainingPackUI->Render();
    }
}
