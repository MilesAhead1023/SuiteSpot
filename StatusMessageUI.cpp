#include "pch.h"
#include "StatusMessageUI.h"

namespace UI {

//
// Show - Display a message with automatic color based on type
//
// This is the main "entry point" for showing messages. You tell it what to say,
// how long to show it, how it should behave, and what kind of message it is.
// The class automatically picks the right color based on the type.
//
void StatusMessage::Show(const std::string& text, float duration, DisplayMode mode, Type type)
{
    text_ = text;
    color_ = GetColorForType(type);
    timer_ = duration;
    maxDuration_ = duration; // Store original duration for fade calculations
    mode_ = mode;
    visible_ = true;
}

//
// ShowCustom - Display a message with a custom color
//
// Same as Show(), but lets you pick your own color instead of using the
// standard Success/Error/Warning/Info colors.
//
void StatusMessage::ShowCustom(const std::string& text, const ImVec4& color, float duration, DisplayMode mode)
{
    text_ = text;
    color_ = color;
    timer_ = duration;
    maxDuration_ = duration;
    mode_ = mode;
    visible_ = true;
}

//
// Convenience methods - Quick shortcuts for common message types
//
// These save you from typing out all the parameters every time.
// Just call ShowSuccess("Done!") instead of Show("Done!", 3.0f, Timer, Success).
//

void StatusMessage::ShowSuccess(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Success);
}

void StatusMessage::ShowError(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Error);
}

void StatusMessage::ShowWarning(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Warning);
}

void StatusMessage::ShowInfo(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Info);
}

void StatusMessage::RenderOverlay(float deltaTime)
{
    if (!visible_) {
        return;
    }

    timer_ -= deltaTime;

    float alpha = 1.0f;
    if (mode_ == DisplayMode::TimerWithFade && maxDuration_ > 0.0f) {
        alpha = timer_ / maxDuration_;
    }

    if (timer_ <= 0.0f && mode_ != DisplayMode::ManualDismiss) {
        visible_ = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    // Anchor to bottom-center, 15% up from bottom edge
    ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.85f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.72f * alpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

    if (ImGui::Begin("##ss_toast", nullptr, flags)) {
        ImVec4 fadedColor = color_;
        fadedColor.w *= alpha;
        ImGui::TextColored(fadedColor, "%s", text_.c_str());

        if (mode_ == DisplayMode::ManualDismiss) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Dismiss")) {
                visible_ = false;
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

//
// IMPORTANT: Call this EVERY FRAME in your UI render function!
// This is where all the magic happens - it draws the message and manages the timer.
//
// How it works:
// 1. If not visible, do nothing (message is hidden)
// 2. If visible, countdown the timer using deltaTime (time since last frame)
// 3. Display the message based on the mode:
//    - Timer: Show until timer expires, then hide instantly
//    - TimerWithFade: Show and gradually fade out as timer approaches 0
//    - ManualDismiss: Show with a "Dismiss" button, ignore timer
//
void StatusMessage::Render(float deltaTime)
{
    if (!visible_) {
        return; // Nothing to show
    }

    // Countdown the timer (subtract time since last frame)
    timer_ -= deltaTime;

    // Handle different display modes
    switch (mode_) {
        case DisplayMode::Timer:
            // Simple timer - show message until timer expires, then hide instantly
            if (timer_ > 0.0f) {
                ImGui::TextColored(color_, "%s", text_.c_str());
            } else {
                visible_ = false; // Timer expired, hide the message
            }
            break;

        case DisplayMode::TimerWithFade:
            // Timer with fade - gradually reduce opacity as timer approaches 0
            if (timer_ > 0.0f) {
                // Calculate fade: when timer = maxDuration, alpha = 1.0 (fully visible)
                //                 when timer = 0, alpha = 0.0 (fully transparent)
                float fadeAlpha = timer_ / maxDuration_;

                // Create faded color by multiplying the alpha channel
                ImVec4 fadedColor = color_;
                fadedColor.w *= fadeAlpha; // w is the alpha channel in ImVec4

                ImGui::TextColored(fadedColor, "%s", text_.c_str());
            } else {
                visible_ = false; // Fade complete, hide the message
            }
            break;

        case DisplayMode::ManualDismiss:
            // Manual dismiss - show message with a "Dismiss" button
            // Timer is ignored in this mode (message stays until user clicks button)
            ImGui::TextColored(color_, "%s", text_.c_str());
            ImGui::SameLine(); // Put the button on the same line as the text

            // Show a small dismiss button next to the message
            if (ImGui::SmallButton("Dismiss")) {
                visible_ = false; // User clicked dismiss, hide the message
            }
            break;
    }
}

//
// Clear - Immediately hide the message
//
// Use this to force the message to disappear right now, regardless of timer or mode.
// It's like hitting a "cancel" button - resets everything back to empty/hidden state.
//
void StatusMessage::Clear()
{
    text_.clear();
    color_ = ImVec4(0, 0, 0, 0); // Transparent black (invisible)
    timer_ = 0.0f;
    maxDuration_ = 0.0f;
    visible_ = false;
}

//
// IsVisible - Check if a message is currently showing
//
// Returns true if there's a message on screen, false if hidden.
//
bool StatusMessage::IsVisible() const
{
    return visible_;
}

//
// GetColorForType - Helper to map message type to color
//
// This is a private helper that returns the standard color for each message type.
// Think of it as a "color palette" for the different message categories.
//
// Colors chosen to match common UI conventions:
// - Success: Green (like a checkmark or "go" signal)
// - Error: Red (like a stop sign or warning light)
// - Warning: Yellow/Orange (like caution tape)
// - Info: Blue (like information signs)
//
ImVec4 StatusMessage::GetColorForType(Type type) const
{
    switch (type) {
        case Type::Success:
            return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Bright green (R=0, G=1, B=0, A=1)

        case Type::Error:
            return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Bright red (R=1, G=0, B=0, A=1)

        case Type::Warning:
            return ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Yellow/Orange (R=1, G=0.8, B=0, A=1)

        case Type::Info:
            return ImVec4(0.4f, 0.7f, 1.0f, 1.0f); // Light blue (R=0.4, G=0.7, B=1, A=1)

        default:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White (fallback, should never happen)
    }
}

} // namespace UI
