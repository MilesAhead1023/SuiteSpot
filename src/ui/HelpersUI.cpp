#include "pch.h"
#include "HelpersUI.h"
#include <algorithm> // for std::clamp

namespace UI {
namespace Helpers {

//
// InputIntWithRange - Smart number input with automatic validation and saving
//
bool InputIntWithRange(const char* label, int& value, int minValue, int maxValue, float width, const char* cvarName,
                       std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                       const char* tooltip, const char* rangeHint, ImGuiInputTextFlags extraFlags)
{
    // Set how wide the input box should be (if a width was specified)
    // This is like saying "make this input box 220 pixels wide"
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }

    // Show the actual input box and check if the user changed the number
    bool changed = ImGui::InputInt(label, &value, 1, 100, extraFlags);
    if (changed) {
        // User typed a new number - make sure it's in the valid range
        // Like making sure someone can't set a delay to -50 seconds or 9999 seconds
        value = std::clamp(value, minValue, maxValue);

        // Save the new value to the plugin's settings file
        // This is what makes the setting persist when you restart the game
        if (cvarName && cvarManager && gameWrapper) {
            SetCVarSafely(cvarName, value, cvarManager, gameWrapper);

            // Immediately persist to config file to prevent settings loss on crash
            cvarManager->executeCommand("writeconfig", false);
        }
    }

    // Show the range hint next to the input (like "0-300s")
    // This reminds users what values are allowed
    if (rangeHint) {
        ImGui::SameLine();
        ImGui::TextDisabled(rangeHint);
    }

    // If there's a tooltip, show it when the user hovers their mouse
    // This is the "helpful hint" feature
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    return changed;
}

//
// ComboWithTooltip - Dropdown menu with automatic help tooltip
//
bool ComboWithTooltip(const char* label, const char* previewValue, const char* tooltip, float width)
{
    // Set how wide the dropdown should be (if a width was specified)
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }

    // Start the dropdown menu
    // BeginCombo returns true if the dropdown is open (user clicked on it)
    bool isOpen = ImGui::BeginCombo(label, previewValue);

    // Show tooltip when hovering over the dropdown
    // This works whether the dropdown is open or closed
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the dropdown is open
    // If true, the caller needs to add items with ImGui::Selectable() and call ImGui::EndCombo()
    return isOpen;
}

//
// ButtonWithTooltip - Button with automatic help tooltip
//
bool ButtonWithTooltip(const char* label, const char* tooltip, const ImVec2& size)
{
    // Create the button and check if it was clicked
    bool clicked = ImGui::Button(label, size);

    // Show tooltip when hovering over the button
    // This lets users know what the button does before clicking
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the button was clicked
    return clicked;
}

//
// CheckboxWithCVar - Checkbox that automatically saves to settings
//
bool CheckboxWithCVar(const char* label, bool& value, const char* cvarName,
                      std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                      const char* tooltip)
{
    // Create the checkbox and check if it was toggled
    bool toggled = ImGui::Checkbox(label, &value);

    if (toggled) {
        // User clicked the checkbox - save the new value to settings
        // This makes the checkbox state persist when you restart the game
        if (cvarName && cvarManager && gameWrapper) {
            SetCVarSafely(cvarName, value, cvarManager, gameWrapper);

            // Immediately persist to config file to prevent settings loss on crash
            cvarManager->executeCommand("writeconfig", false);
        }
    }

    // Show tooltip when hovering over the checkbox
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the checkbox was toggled
    return toggled;
}

//
// InputTextWithTooltip - Text input box with automatic tooltip
//
bool InputTextWithTooltip(const char* label, char* buf, size_t bufSize, const char* tooltip, float width,
                          ImGuiInputTextFlags flags)
{
    // Set how wide the input box should be (if a width was specified)
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }

    // Show the text input box and check if the user modified the text
    bool changed = ImGui::InputText(label, buf, bufSize, flags);

    // Show tooltip when hovering over the input box
    // Helps users understand what to type
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the text was modified
    return changed;
}

//
// ExecuteCommandSafely - Execute command on game thread
//
void ExecuteCommandSafely(std::shared_ptr<GameWrapper> gameWrapper, std::shared_ptr<CVarManagerWrapper> cvarManager,
                          const std::string& command, float delay)
{
    // Safety checks
    if (!gameWrapper || !cvarManager) return;

    // Schedule command execution on game thread to avoid crashes
    gameWrapper->SetTimeout([cvarManager, command](GameWrapper* gw) { cvarManager->executeCommand(command); }, delay);
}

} // namespace Helpers
} // namespace UI
