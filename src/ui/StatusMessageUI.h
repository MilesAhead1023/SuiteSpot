#pragma once

// StatusMessage.h
//
// A reusable class for displaying temporary or dismissible status messages in the UI.
// This replaces 3 different timer/boolean patterns found in LoadoutUI, SettingsUI, and TrainingPackUI.
//
// Purpose: Provides a single, consistent way to show feedback messages to users
// (like "Loadout applied!", "Pack added!", "Error: invalid code") across all UI components.
//
// Think of it like a notification system - you can show a message, pick how long it shows,
// and choose whether it fades out automatically or waits for the user to dismiss it.
//
// Example Usage:
//   // In your UI class header file:
//   UI::StatusMessage statusMsg;
//
//   // When something happens (like saving successfully):
//   statusMsg.ShowSuccess("Loadout applied!", 3.0f);
//
//   // Every frame in your render function:
//   statusMsg.Render(ImGui::GetIO().DeltaTime);
//
// The class handles all the timer logic, color management, and display automatically.

#include "imgui.h"
#include <string>

namespace UI {

class StatusMessage
{
  public:
    //
    // DisplayMode - How the message should appear and disappear
    //
    // This controls the "lifecycle" of the message - how long it shows and how it hides.
    //
    enum class DisplayMode
    {
        // Timer - Shows for a set duration, then disappears instantly
        // Like a toast notification that just vanishes after 3 seconds
        // Best for: Quick confirmations like "Saved!" or "Refreshed"
        Timer,

        // TimerWithFade - Shows for a set duration, gradually fading to transparent
        // The message starts fully visible, then slowly becomes see-through before vanishing
        // Best for: Success messages that don't need immediate attention
        TimerWithFade,

        // ManualDismiss - Stays visible until user clicks a "Dismiss" button
        // Won't go away on its own - the user must actively close it
        // Best for: Important errors or warnings that need acknowledgment
        ManualDismiss
    };

    //
    // Type - The kind of message (determines the default color)
    //
    // This helps categorize messages and automatically picks an appropriate color.
    // Think of it like traffic lights: green = good, red = bad, yellow = warning.
    //
    enum class Type
    {
        Success, // Something worked! (Green color)
        Error,   // Something failed! (Red color)
        Warning, // Be careful! (Yellow color)
        Info     // Just letting you know... (Blue color)
    };

    // Constructor - creates an empty status message (nothing shows until you call Show...)
    StatusMessage() = default;

    //
    // Show - Display a message with automatic color based on type
    //
    // This is the main method for showing messages. It picks the color automatically
    // based on whether it's a success, error, warning, or info message.
    //
    // Parameters:
    //   text - The message to display (e.g., "Loadout applied successfully!")
    //   duration - How many seconds to show it (ignored if mode is ManualDismiss)
    //   mode - How it should appear/disappear (Timer, TimerWithFade, or ManualDismiss)
    //   type - What kind of message it is (Success, Error, Warning, or Info)
    //
    // Example:
    //   statusMsg.Show("Settings saved!", 3.0f, DisplayMode::Timer, Type::Success);
    //
    void Show(const std::string& text, float duration, DisplayMode mode, Type type);

    //
    // ShowCustom - Display a message with a custom color
    //
    // Use this when you want a specific color that doesn't match the standard types.
    // For example, if you want purple text or a specific shade of blue.
    //
    // Parameters:
    //   text - The message to display
    //   color - Custom ImVec4 color (RGBA values from 0.0 to 1.0)
    //   duration - How many seconds to show it (ignored if mode is ManualDismiss)
    //   mode - How it should appear/disappear
    //
    // Example:
    //   ImVec4 purple = ImVec4(0.8f, 0.4f, 1.0f, 1.0f);
    //   statusMsg.ShowCustom("Custom message!", purple, 5.0f, DisplayMode::TimerWithFade);
    //
    void ShowCustom(const std::string& text, const ImVec4& color, float duration, DisplayMode mode);

    //
    // Convenience methods - Shortcuts for common message types
    //
    // These are "helper methods" that make showing messages even easier.
    // Instead of typing out all the parameters, you can just call ShowSuccess(...)
    // and it automatically picks sensible defaults.
    //

    // ShowSuccess - Show a green success message
    // Defaults: 3 seconds, simple timer (instant hide)
    // Example: statusMsg.ShowSuccess("Pack added!");
    void ShowSuccess(const std::string& text, float duration = 3.0f, DisplayMode mode = DisplayMode::Timer);

    // ShowError - Show a red error message
    // Defaults: 3 seconds, manual dismiss (user must click "Dismiss")
    // Example: statusMsg.ShowError("Invalid pack code format");
    void ShowError(const std::string& text, float duration = 3.0f, DisplayMode mode = DisplayMode::ManualDismiss);

    // ShowWarning - Show a yellow warning message
    // Defaults: 5 seconds (longer than success), simple timer
    // Example: statusMsg.ShowWarning("Applying loadout...");
    void ShowWarning(const std::string& text, float duration = 5.0f, DisplayMode mode = DisplayMode::Timer);

    // ShowInfo - Show a blue informational message
    // Defaults: 2.5 seconds, fade effect (looks smooth)
    // Example: statusMsg.ShowInfo("Loadouts refreshed");
    void ShowInfo(const std::string& text, float duration = 2.5f, DisplayMode mode = DisplayMode::TimerWithFade);

    //
    // Render - Display the message on screen
    //
    // IMPORTANT: You must call this EVERY FRAME in your UI render function!
    // This is what actually draws the message and handles the timer countdown.
    //
    // If you don't call this, the message won't show up.
    // Think of it like watering a plant - you need to do it regularly.
    //
    // Parameters:
    //   deltaTime - Time since last frame in seconds (use ImGui::GetIO().DeltaTime)
    //
    // Example in your render function:
    //   void RenderMyUI() {
    //       // ... your other UI code ...
    //       statusMsg.Render(ImGui::GetIO().DeltaTime);
    //   }
    //
    void Render(float deltaTime);

    //
    // RenderOverlay - Display the message as a floating on-screen overlay
    //
    // Use this when calling from outside an ImGui window context (e.g., BakkesMod's
    // Render() callback). Creates its own semi-transparent window anchored to the
    // bottom-center of the screen. Safe to call every frame even when not visible.
    //
    // Parameters:
    //   deltaTime - Time since last frame in seconds (use ImGui::GetIO().DeltaTime)
    //
    void RenderOverlay(float deltaTime);

    //
    // Clear - Immediately hide the message
    //
    // Use this when you want to force the message to disappear right now,
    // regardless of its timer or mode. Like pressing a "cancel" button.
    //
    // Example:
    //   if (userClickedCancel) {
    //       statusMsg.Clear();  // Hide the message instantly
    //   }
    //
    void Clear();

    //
    // IsVisible - Check if a message is currently showing
    //
    // Returns true if there's a message on screen, false if it's hidden.
    // Useful for checking if you need to show a new message or if one is already active.
    //
    // Example:
    //   if (!statusMsg.IsVisible()) {
    //       statusMsg.ShowSuccess("New message!");
    //   }
    //
    bool IsVisible() const;

  private:
    // Internal state - these track what message is showing and how
    std::string text_;  // The current message text
    ImVec4 color_;      // The current message color (RGBA)
    float timer_;       // Countdown timer (seconds remaining)
    float maxDuration_; // Original duration (used for fade calculation)
    DisplayMode mode_;  // How the message should behave
    bool visible_;      // Is the message currently showing?

    // Helper method to get the standard color for a message type
    // (e.g., Type::Success returns green, Type::Error returns red)
    ImVec4 GetColorForType(Type type) const;
};

} // namespace UI
