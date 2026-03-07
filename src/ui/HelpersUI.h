#pragma once

// UIHelpers.h
//
// This file contains "helper functions" that make the UI code cleaner.
// Instead of writing the same 5-10 lines of code over and over for every button or input box,
// we can call one of these helpers that does all the work in one line.
//
// Think of it like having pre-made "templates" for common UI elements:
// - Input boxes that automatically validate numbers and save settings
// - Buttons that show helpful tooltips when you hover
// - Dropdown menus with tooltips
// - Status messages that fade out automatically
//
// This makes the code easier to read, maintain, and less error-prone.

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "imgui.h"
#include <string>

namespace UI {
namespace Helpers {

//
// SetCVarSafely - Safely saves a setting value to the plugin
//
// This is a utility function that saves values to BakkesMod's settings system (CVars).
// It includes safety checks to prevent crashes if the settings system isn't available.
//
// Think of CVars like a settings file - when you change a value in the UI, this saves it
// so it persists when you restart Rocket League.
//
// Parameters:
//   cvarName - The internal name of the setting (e.g., "suitespot_delay_queue_sec")
//   value - The value to save (can be int, bool, float, string, etc.)
//   cvarManager - The plugin's settings manager (handles the actual saving)
//
template <typename T>
void SetCVarSafely(const char* cvarName, const T& value, std::shared_ptr<CVarManagerWrapper> cvarManager,
                   std::shared_ptr<GameWrapper> gameWrapper)
{
    // Safety check: Make sure the managers exist
    if (!cvarManager || !gameWrapper) return;

    // Get the setting
    auto cvar = cvarManager->getCvar(cvarName);
    if (cvar) {
        // CRITICAL: Force the update to the Game Thread
        // This prevents "Rendering thread exception" when called from UI/Render threads
        gameWrapper->SetTimeout(
            [cvar, value](GameWrapper* gw) {
                // Use a copy of the value and the cvar wrapper inside the lambda
                CVarWrapper(cvar).setValue(value);
            },
            0.0f);
    }
}

//
// InputIntWithRange - Creates a "smart" number input box
//
bool InputIntWithRange(const char* label, int& value, int minValue, int maxValue, float width, const char* cvarName,
                       std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                       const char* tooltip = nullptr, const char* rangeHint = nullptr,
                       ImGuiInputTextFlags extraFlags = 0);

//
// ComboWithTooltip - Creates a dropdown menu with automatic tooltip
//
bool ComboWithTooltip(const char* label, const char* previewValue, const char* tooltip, float width = 0.0f);

//
// ButtonWithTooltip - Creates a button with automatic tooltip
//
bool ButtonWithTooltip(const char* label, const char* tooltip, const ImVec2& size = ImVec2(0, 0));

//
// CheckboxWithCVar - Creates a checkbox that automatically saves to settings
//
bool CheckboxWithCVar(const char* label, bool& value, const char* cvarName,
                      std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                      const char* tooltip = nullptr);

//
// InputTextWithTooltip - Creates a text input box with automatic tooltip
//
// Creates a text input field that automatically shows a tooltip when you hover over it.
// Cleaner than writing the input and tooltip code separately.
//
// Example: "Training Map Code" input with tooltip "Enter the code (e.g., 555F-7503-BBB9-E1E3)"
//
// Parameters:
//   label - What to display next to the input box
//   buf - Character buffer for the text being edited
//   bufSize - Size of the buffer (how many characters it can hold)
//   tooltip - Help text displayed when hovering
//   width - How wide the input box should be in pixels (0 = use default width)
//   flags - Special input options (optional - see ImGui docs)
//
// Returns: true if the text was modified
//
// Usage:
//   char mapCode[64] = "";
//   if (InputTextWithTooltip("Code", mapCode, sizeof(mapCode),
//                            "Enter pack code", 220.0f)) {
//       // Text was changed
//   }
//
bool InputTextWithTooltip(const char* label, char* buf, size_t bufSize, const char* tooltip, float width = 0.0f,
                          ImGuiInputTextFlags flags = 0);

//
// ExecuteCommandSafely - Execute BakkesMod command on game thread
//
// Safely executes a command by scheduling it on the game thread to avoid crashes.
// This is the standard way to execute commands from UI or render threads.
//
// Parameters:
//   gameWrapper - The game wrapper (for SetTimeout)
//   cvarManager - The CVar manager (for executeCommand)
//   command - The command to execute (e.g., "load_freeplay 555F-7503-BBB9-E1E3")
//   delay - Optional delay in seconds before execution (default: 0.0f)
//
// Usage:
//   ExecuteCommandSafely(gameWrapper, cvarManager, "load_freeplay ABC123");
//
void ExecuteCommandSafely(std::shared_ptr<GameWrapper> gameWrapper, std::shared_ptr<CVarManagerWrapper> cvarManager,
                          const std::string& command, float delay = 0.0f);

} // namespace Helpers
} // namespace UI
