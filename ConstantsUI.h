#pragma once
#include "IMGUI/imgui.h"

// Centralized UI constants for SuiteSpot plugin
// Each constant is named specifically for the UI element it controls
// for maximum granularity and ease of individual component editing

namespace UI {

// ===================================================================
// GLOBAL UI CONSTANTS
// ===================================================================

// Font scale applied to all SuiteSpot windows (per-window, not global)
constexpr float FONT_SCALE = 1.12f;

// === Interactive Control Styling ===

// Border thickness (px) on buttons, checkboxes, inputs, sliders, combos
constexpr float INTERACTIVE_FRAME_BORDER_SIZE = 1.0f;

// Corner rounding (px) on all framed interactive controls
constexpr float INTERACTIVE_FRAME_ROUNDING = 3.0f;

// Border color for interactive controls — soft blue accent
inline const ImVec4 INTERACTIVE_BORDER_COLOR = ImVec4(0.40f, 0.55f, 0.80f, 0.85f);

// Padding inside framed controls (buttons, inputs, combos) — wider than ImGui default (4,3)
inline const ImVec2 FRAME_PADDING = {8.0f, 4.0f};

// Spacing between consecutive items — more vertical breathing room than default (8,4)
inline const ImVec2 ITEM_SPACING = {8.0f, 6.0f};

// Inner padding of child/popup windows — slightly more horizontal than default (8,8)
inline const ImVec2 WINDOW_PADDING = {10.0f, 8.0f};

// Corner rounding (px) for child panel borders — matches FrameRounding
constexpr float CHILD_ROUNDING = 4.0f;

// Border size for child windows (panels, sub-panels) — separate from FrameBorderSize
constexpr float CHILD_BORDER_SIZE = 1.0f;

// Corner rounding for tab items
constexpr float TAB_ROUNDING = 3.0f;

// Corner rounding for slider/scrollbar grab handles
constexpr float GRAB_ROUNDING = 2.0f;

// Minimum pixel size for slider grab handles
constexpr float GRAB_MIN_SIZE = 10.0f;

// Corner rounding for scrollbar handles
constexpr float SCROLLBAR_ROUNDING = 3.0f;

// Inner spacing inside composed widgets (checkbox+label, combo+label etc.)
inline const ImVec2 ITEM_INNER_SPACING = {6.0f, 4.0f};

// Selectable text alignment — vertically centered
inline const ImVec2 SELECTABLE_TEXT_ALIGN = {0.0f, 0.5f};

// Controlled indent spacing for sub-sections
constexpr float INDENT_SPACING = 14.0f;

// Status warning color (orange — "no map selected", missing inputs)
inline const ImVec4 STATUS_WARN_COLOR = ImVec4(1.0f, 0.65f, 0.0f, 1.0f);

// Section header accent color — bright blue, used by DrawSectionHeader helper
inline const ImVec4 SECTION_HEADER_COLOR = ImVec4(0.45f, 0.78f, 1.0f, 1.0f);

// Active/pressed map mode button highlight
inline const ImVec4 MAP_MODE_ACTIVE_COLOR = ImVec4(0.22f, 0.50f, 0.82f, 1.00f);

// === Scrollbar styling ===
inline const ImVec4 SCROLLBAR_BG_COLOR = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
inline const ImVec4 SCROLLBAR_GRAB_COLOR = ImVec4(0.22f, 0.28f, 0.40f, 1.00f);
inline const ImVec4 SCROLLBAR_GRAB_HOVER_COLOR = ImVec4(0.30f, 0.40f, 0.58f, 1.00f);
inline const ImVec4 SCROLLBAR_GRAB_ACTIVE_COLOR = ImVec4(0.38f, 0.52f, 0.72f, 1.00f);

// Text selection highlight in inputs
inline const ImVec4 TEXT_SELECTED_BG_COLOR = ImVec4(0.20f, 0.40f, 0.70f, 0.40f);

// Popup/dropdown background — darker for depth
inline const ImVec4 POPUP_BG_COLOR = ImVec4(0.08f, 0.09f, 0.13f, 0.96f);

// Keyboard navigation highlight
inline const ImVec4 NAV_HIGHLIGHT_COLOR = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);

// Separator line color — soft accent blue
inline const ImVec4 SEPARATOR_COLOR = ImVec4(0.28f, 0.45f, 0.65f, 0.70f);

// --- Button colors (visible dark-blue-slate so buttons stand out from window bg) ---
inline const ImVec4 BUTTON_COLOR = ImVec4(0.22f, 0.28f, 0.40f, 0.80f);
inline const ImVec4 BUTTON_HOVER_COLOR = ImVec4(0.30f, 0.40f, 0.58f, 0.90f);
inline const ImVec4 BUTTON_ACTIVE_COLOR = ImVec4(0.38f, 0.52f, 0.72f, 1.00f);

// --- Frame background colors for inputs, checkboxes, combos ---
inline const ImVec4 FRAME_BG_COLOR = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
inline const ImVec4 FRAME_BG_HOVER_COLOR = ImVec4(0.22f, 0.26f, 0.34f, 1.00f);
inline const ImVec4 FRAME_BG_ACTIVE_COLOR = ImVec4(0.28f, 0.34f, 0.46f, 1.00f);

// CheckMark color drawn inside checkboxes
inline const ImVec4 CHECKMARK_COLOR = ImVec4(0.65f, 0.88f, 1.00f, 1.00f);

// --- Selectable / list-row highlight colors (Header in ImGui parlance) ---
inline const ImVec4 HEADER_COLOR = ImVec4(0.25f, 0.40f, 0.60f, 0.65f);
inline const ImVec4 HEADER_HOVER_COLOR = ImVec4(0.30f, 0.48f, 0.68f, 0.75f);
inline const ImVec4 HEADER_ACTIVE_COLOR = ImVec4(0.35f, 0.55f, 0.78f, 1.00f);

// --- Tab colors ---
inline const ImVec4 TAB_COLOR = ImVec4(0.16f, 0.18f, 0.24f, 0.90f);
inline const ImVec4 TAB_HOVER_COLOR = ImVec4(0.28f, 0.38f, 0.52f, 1.00f);
inline const ImVec4 TAB_ACTIVE_COLOR = ImVec4(0.22f, 0.32f, 0.48f, 1.00f);
inline const ImVec4 TAB_UNFOCUSED_COLOR = ImVec4(0.12f, 0.14f, 0.18f, 0.80f);
inline const ImVec4 TAB_UNFOCUSED_ACTIVE_COLOR = ImVec4(0.18f, 0.24f, 0.36f, 1.00f);

// Child window background — slightly darker than outer window for depth
inline const ImVec4 CHILD_BG_COLOR = ImVec4(0.10f, 0.11f, 0.15f, 0.60f);

// ===================================================================
// SETTINGS UI CONSTANTS
// ===================================================================
namespace SettingsUI {

// === Dropdown Widths ===

// Width of the "Freeplay Maps" dropdown in Map Selection tab
constexpr float FREEPLAY_MAPS_DROPDOWN_WIDTH = 260.0f;

// Width of the "Training Packs" dropdown in Map Selection tab
constexpr float TRAINING_PACKS_DROPDOWN_WIDTH = 260.0f;

// Width of the "Workshop Maps" dropdown in Map Selection tab
constexpr float WORKSHOP_MAPS_DROPDOWN_WIDTH = 260.0f;

// === Input Box Widths ===

// Width of the "Delay Freeplay (sec)" input box in Map Selection tab
constexpr float DELAY_FREEPLAY_INPUT_WIDTH = 220.0f;

// Width of the "Delay Training (sec)" input box in Map Selection tab
constexpr float DELAY_TRAINING_INPUT_WIDTH = 220.0f;

// Width of the "Delay Workshop (sec)" input box in Map Selection tab
constexpr float DELAY_WORKSHOP_INPUT_WIDTH = 220.0f;

// Width of the "Delay Queue (sec)" input box in Auto-Queue tab
constexpr float DELAY_QUEUE_INPUT_WIDTH = 220.0f;

// Width of the "Workshop maps root folder" input in Workshop Source section
constexpr float WORKSHOP_PATH_INPUT_WIDTH = 420.0f;

// === Range Limits (Min/Max Values) ===

// Minimum value for Freeplay delay slider (seconds)
constexpr int DELAY_FREEPLAY_MIN_SECONDS = 0;

// Maximum value for Freeplay delay slider (seconds)
constexpr int DELAY_FREEPLAY_MAX_SECONDS = 300;

// Minimum value for Training delay slider (seconds)
constexpr int DELAY_TRAINING_MIN_SECONDS = 0;

// Maximum value for Training delay slider (seconds)
constexpr int DELAY_TRAINING_MAX_SECONDS = 300;

// Minimum value for Workshop delay slider (seconds)
constexpr int DELAY_WORKSHOP_MIN_SECONDS = 0;

// Maximum value for Workshop delay slider (seconds)
constexpr int DELAY_WORKSHOP_MAX_SECONDS = 300;

// Minimum value for Queue delay slider (seconds)
constexpr int DELAY_QUEUE_MIN_SECONDS = 0;

// Maximum value for Queue delay slider (seconds)
constexpr int DELAY_QUEUE_MAX_SECONDS = 300;

// === Positions and Spacing ===

// Horizontal position for status text display in General tab
constexpr float STATUS_TEXT_POSITION_X = 420.0f;

// Spacing between radio buttons in Map Type selection
constexpr float MAP_TYPE_RADIO_BUTTON_SPACING = 16.0f;

// === Status Colors ===

// Text color for section headers in settings (brightened from 0.6)
inline const ImVec4 HEADER_TEXT_COLOR = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

// Color for separator line in status section (white)
inline const ImVec4 STATUS_SEPARATOR_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

// Text color for "Enabled" status in Auto-Load settings (bright green)
inline const ImVec4 STATUS_ENABLED_TEXT_COLOR = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);

// Text color for "Disabled" status in Auto-Load settings (bright red)
inline const ImVec4 STATUS_DISABLED_TEXT_COLOR = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

// === Timer Durations ===

// Duration (seconds) for success message fadeout after adding custom training pack
constexpr float CUSTOM_PACK_SUCCESS_MESSAGE_DURATION = 3.0f;

// Duration (seconds) for error message fadeout in workshop path validation
constexpr float WORKSHOP_PATH_ERROR_MESSAGE_DURATION = 2.0f;

// Divisor used to calculate alpha fade effect for success messages (timer / divisor)
constexpr float SUCCESS_MESSAGE_FADE_DIVISOR = 3.0f;

// === Message Colors ===

// Text color for success message "Pack added!" in custom training form (green)
inline const ImVec4 CUSTOM_PACK_SUCCESS_TEXT_COLOR = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

// === Hotkeys Tab ===

// Width of the label column in the Hotkeys tab
constexpr float HOTKEY_LABEL_COL_WIDTH = 150.0f;

// Width of each key name input field in the Hotkeys tab
constexpr float HOTKEY_KEY_INPUT_WIDTH = 110.0f;

struct HotkeyRow
{
    const char* label;
    const char* key1CVar;
    const char* key2CVar;
};

static const HotkeyRow HOTKEY_ROWS[] = {
    {"Cycle Mode Fwd", "suitespot_hotkey_map_mode_fwd_key", "suitespot_hotkey_map_mode_fwd_key2"},
    {"Cycle Mode Back", "suitespot_hotkey_map_mode_bk_key", "suitespot_hotkey_map_mode_bk_key2"},
    {"Cycle Map Fwd", "suitespot_hotkey_cycle_map_fwd_key", "suitespot_hotkey_cycle_map_fwd_key2"},
    {"Cycle Map Back", "suitespot_hotkey_cycle_map_bk_key", "suitespot_hotkey_cycle_map_bk_key2"},
    {"Load Now", "suitespot_hotkey_load_now_key", "suitespot_hotkey_load_now_key2"},
};

} // namespace SettingsUI

// ===================================================================
// TRAINING PACK UI CONSTANTS
// ===================================================================
namespace TrainingPackUI {

// === Filter Layout - Responsive Widths ===

// Percentage of available width for Search filter box in Training Packs tab (40%)
constexpr float FILTER_SEARCH_WIDTH_PERCENT = 0.40f;

// Percentage of available width for Difficulty filter dropdown in Training Packs tab (25%)
constexpr float FILTER_DIFFICULTY_WIDTH_PERCENT = 0.25f;

// Percentage of available width for Shots filter in Training Packs tab (35%)
constexpr float FILTER_SHOTS_WIDTH_PERCENT = 0.35f;

// Minimum pixel width for Search filter box (used when window is too narrow)
constexpr float FILTER_SEARCH_MIN_WIDTH = 150.0f;

// Minimum pixel width for Difficulty filter dropdown (used when window is too narrow)
constexpr float FILTER_DIFFICULTY_MIN_WIDTH = 120.0f;

// Minimum pixel width for Shots filter (used when window is too narrow)
constexpr float FILTER_SHOTS_MIN_WIDTH = 150.0f;

// === Dropdowns ===

// Width of the tag filter dropdown in Training Packs tab
constexpr float TAG_FILTER_DROPDOWN_WIDTH = 200.0f;

// === Positions and Spacing ===

// Button group width offset from right edge of window
constexpr float BUTTON_GROUP_OFFSET_FROM_RIGHT = 280.0f;

// Indentation amount for custom pack form section
constexpr float CUSTOM_PACK_FORM_INDENT = 10.0f;

// === Table Configuration ===

// Number of columns in Training Packs table
constexpr int TABLE_COLUMN_COUNT = 5;

// Minimum width for any column in Training Packs table (before dynamic sizing)
constexpr float TABLE_MIN_COLUMN_WIDTH = 40.0f;

// Extra padding added to calculated column widths in Training Packs table
constexpr float TABLE_COLUMN_PADDING = 20.0f;

// Default width for Actions column in Training Packs table
constexpr float TABLE_ACTIONS_COLUMN_WIDTH = 200.0f;

// Maximum width cap for Name column in Training Packs table
constexpr float TABLE_NAME_COLUMN_MAX_WIDTH = 400.0f;

// === Custom Pack Form - Input Widths ===

// Width of "Training Map Code" input field in custom pack form
constexpr float CUSTOM_PACK_CODE_INPUT_WIDTH = 220.0f;

// Width of "Training Map Name" input field in custom pack form
constexpr float CUSTOM_PACK_NAME_INPUT_WIDTH = 300.0f;

// Width of "Creator" input field in custom pack form
constexpr float CUSTOM_PACK_CREATOR_INPUT_WIDTH = 200.0f;

// Width of "Tags" input field in custom pack form
constexpr float CUSTOM_PACK_TAGS_INPUT_WIDTH = 300.0f;

// Width of "Video URL" input field in custom pack form
constexpr float CUSTOM_PACK_VIDEO_URL_INPUT_WIDTH = 350.0f;

// Width of "Notes" multiline text area in custom pack form
constexpr float CUSTOM_PACK_NOTES_INPUT_WIDTH = 400.0f;

// Height of "Notes" multiline text area in custom pack form
constexpr float CUSTOM_PACK_NOTES_INPUT_HEIGHT = 60.0f;

// === Custom Pack Form - Dropdown Widths ===

// Width of the difficulty dropdown in custom pack form
constexpr float CUSTOM_PACK_DIFFICULTY_DROPDOWN_WIDTH = 150.0f;

// === Custom Pack Form - Button Sizes ===

// Width of "Add Pack" button in custom pack form
constexpr float CUSTOM_PACK_ADD_BUTTON_WIDTH = 100.0f;

// Height of "Add Pack" button in custom pack form (0 = auto)
constexpr float CUSTOM_PACK_ADD_BUTTON_HEIGHT = 0.0f;

// Width of "Clear" button in custom pack form
constexpr float CUSTOM_PACK_CLEAR_BUTTON_WIDTH = 80.0f;

// Height of "Clear" button in custom pack form (0 = auto)
constexpr float CUSTOM_PACK_CLEAR_BUTTON_HEIGHT = 0.0f;

// === Shot Count Ranges ===

// Minimum value for "Min Shots" filter slider in Training Packs tab
constexpr int FILTER_MIN_SHOTS_MIN = 0;

// Maximum value for "Min Shots" filter slider in Training Packs tab
constexpr int FILTER_MIN_SHOTS_MAX = 50;

// Minimum value for shot count slider in custom pack creation form
constexpr int CUSTOM_PACK_SHOTS_MIN = 1;

// Maximum value for shot count slider in custom pack creation form
constexpr int CUSTOM_PACK_SHOTS_MAX = 50;

// === Validation Constants ===

// Expected character length for properly formatted training pack code
constexpr int PACK_CODE_EXPECTED_LENGTH = 19;

// Position of first dash in training pack code (0-indexed)
constexpr int PACK_CODE_DASH_POSITION_1 = 4;

// Position of second dash in training pack code (0-indexed)
constexpr int PACK_CODE_DASH_POSITION_2 = 9;

// Position of third dash in training pack code (0-indexed)
constexpr int PACK_CODE_DASH_POSITION_3 = 14;

// Maximum characters allowed for raw pack code before formatting
constexpr int PACK_CODE_RAW_MAX_LENGTH = 16;

// === Difficulty Badge Colors ===

// Background color for Unranked/Unknown difficulty badge
inline const ImVec4 DIFFICULTY_BADGE_UNRANKED_COLOR = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);

// Background color for Bronze difficulty badge (#925732)
inline const ImVec4 DIFFICULTY_BADGE_BRONZE_COLOR = ImVec4(0.57f, 0.34f, 0.20f, 1.0f);

// Background color for Silver difficulty badge (#91969B)
inline const ImVec4 DIFFICULTY_BADGE_SILVER_COLOR = ImVec4(0.57f, 0.59f, 0.61f, 1.0f);

// Background color for Gold difficulty badge (#C3A11C)
inline const ImVec4 DIFFICULTY_BADGE_GOLD_COLOR = ImVec4(0.76f, 0.63f, 0.11f, 1.0f);

// Background color for Platinum difficulty badge (#43F8F0)
inline const ImVec4 DIFFICULTY_BADGE_PLATINUM_COLOR = ImVec4(0.26f, 0.97f, 0.94f, 1.0f);

// Background color for Diamond difficulty badge (#00BBFF)
inline const ImVec4 DIFFICULTY_BADGE_DIAMOND_COLOR = ImVec4(0.00f, 0.73f, 1.00f, 1.0f);

// Background color for Champion difficulty badge (#990099)
inline const ImVec4 DIFFICULTY_BADGE_CHAMPION_COLOR = ImVec4(0.60f, 0.00f, 0.60f, 1.0f);

// Background color for Grand Champion difficulty badge (#FC272F)
inline const ImVec4 DIFFICULTY_BADGE_GRAND_CHAMPION_COLOR = ImVec4(0.99f, 0.15f, 0.18f, 1.0f);

// Background color for Supersonic Legend difficulty badge (#F6FAFF)
inline const ImVec4 DIFFICULTY_BADGE_SUPERSONIC_LEGEND_COLOR = ImVec4(0.96f, 0.98f, 1.00f, 1.0f);

// === UI State Colors ===

// Text color for section headers in Training Packs tab (bright cyan)
inline const ImVec4 SECTION_HEADER_TEXT_COLOR = ImVec4(0.4f, 0.9f, 1.0f, 1.0f);

// Text color for "Last updated" timestamp (brightened gray)
inline const ImVec4 LAST_UPDATED_TEXT_COLOR = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

// Text color for scraping status message (bright yellow)
inline const ImVec4 SCRAPING_STATUS_TEXT_COLOR = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);

// Text color for success message "Pack added!" in custom pack form (bright green)
inline const ImVec4 SUCCESS_MESSAGE_TEXT_COLOR = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);

// Text color for error message in pack code validation (bright red)
inline const ImVec4 ERROR_MESSAGE_TEXT_COLOR = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

// Text color for disabled/info text in custom pack form (brightened gray)
inline const ImVec4 DISABLED_INFO_TEXT_COLOR = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

// === Button Colors ===

// Background color for "In Bag" badge indicator (green)
inline const ImVec4 IN_BAG_BUTTON_BG_COLOR = ImVec4(0.2f, 0.6f, 0.2f, 1.0f);

// Background color for "Delete" button in Training Packs table (red)
inline const ImVec4 DELETE_BUTTON_BG_COLOR = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);

// Background color for "Delete" button when hovered (lighter red)
inline const ImVec4 DELETE_BUTTON_HOVER_COLOR = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);

} // namespace TrainingPackUI

// ===================================================================
// LOADOUT UI CONSTANTS
// ===================================================================
namespace LoadoutUI {

// === Dropdown Widths ===

// Width of the loadout selection dropdown in Loadout Manager tab
constexpr float LOADOUT_SELECTOR_DROPDOWN_WIDTH = 220.0f;

// === Timer Durations ===

// Duration (seconds) for "Applying loadout..." status message
constexpr float APPLYING_STATUS_DURATION = 5.0f;

// Duration (seconds) for "Loadout applied" success message
constexpr float SUCCESS_MESSAGE_DURATION = 3.0f;

// Duration (seconds) for "Loadouts refreshed" message
constexpr float REFRESH_MESSAGE_DURATION = 2.5f;

// === Status Message Colors ===

// Text color for section header in Loadout Manager tab (bright cyan)
inline const ImVec4 SECTION_HEADER_COLOR = ImVec4(0.4f, 0.9f, 1.0f, 1.0f);

// Text color for error/warning messages (bright red)
inline const ImVec4 ERROR_WARNING_TEXT_COLOR = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);

// Text color for "Applying loadout..." status (yellow)
inline const ImVec4 APPLYING_STATUS_COLOR = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

// Text color for success message "Loadout applied" (green)
inline const ImVec4 SUCCESS_MESSAGE_COLOR = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);

// Text color for "Loadouts refreshed" message (blue)
inline const ImVec4 REFRESH_MESSAGE_COLOR = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);

} // namespace LoadoutUI

// ===================================================================
// QUICK PICKS UI CONSTANTS (Two-Panel Layout)
// ===================================================================
namespace QuickPicksUI {
// Percentage of available width for left panel (pack list)
constexpr float LEFT_PANEL_WIDTH_PERCENT = 0.40f;

// Minimum width for left panel
constexpr float LEFT_PANEL_MIN_WIDTH = 200.0f;

// Height of the two-panel browser area
constexpr float BROWSER_HEIGHT = 400.0f;

// Colors
inline const ImVec4 PACK_NAME_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 CREATOR_COLOR = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
inline const ImVec4 DESCRIPTION_COLOR = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
inline const ImVec4 SELECTED_BADGE_COLOR = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
} // namespace QuickPicksUI

// ===================================================================
// WORKSHOP BROWSER UI CONSTANTS (Two-Panel Layout)
// ===================================================================
namespace WorkshopBrowserUI {
// Percentage of available width for left panel (map list)
constexpr float LEFT_PANEL_WIDTH_PERCENT = 0.40f;

// Minimum width for left panel
constexpr float LEFT_PANEL_MIN_WIDTH = 200.0f;

// Height of the two-panel browser area
constexpr float BROWSER_HEIGHT = 400.0f;

// Preview image dimensions
constexpr float PREVIEW_IMAGE_WIDTH = 280.0f;
constexpr float PREVIEW_IMAGE_HEIGHT = 158.0f; // 16:9 aspect ratio

// Colors
inline const ImVec4 MAP_NAME_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 AUTHOR_COLOR = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
inline const ImVec4 DESCRIPTION_COLOR = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
inline const ImVec4 NO_MAPS_COLOR = ImVec4(0.8f, 0.8f, 0.3f, 1.0f);
inline const ImVec4 SELECTED_BADGE_COLOR = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
} // namespace WorkshopBrowserUI

// ===================================================================
// PACK BROWSER UI CONSTANTS (Two-Panel Layout — floating window)
// ===================================================================
namespace PackBrowserUI {
// Left panel takes 60% of available width (wider list to show names)
constexpr float LEFT_PANEL_WIDTH_PERCENT = 0.60f;

// Minimum pixel width for left panel before clamping
constexpr float LEFT_PANEL_MIN_WIDTH = 320.0f;

// Height of the two-panel browser area (shrinks to allow header/footer)
constexpr float BROWSER_HEIGHT = -60.0f; // negative = fill to bottom minus N px

// YouTube thumbnail dimensions (standard mqdefault.jpg)
constexpr float THUMBNAIL_WIDTH = 280.0f;
constexpr float THUMBNAIL_HEIGHT = 157.5f; // 16:9

// Action button height in detail panel
constexpr float ACTION_BUTTON_HEIGHT = 26.0f;

// Tag badge padding (horizontal, vertical)
inline const ImVec2 TAG_BADGE_PADDING = ImVec2(6.0f, 2.0f);

// Colors — detail panel
inline const ImVec4 PACK_NAME_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 CREATOR_COLOR = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
inline const ImVec4 STATS_COLOR = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
inline const ImVec4 COMMENTS_COLOR = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
inline const ImVec4 SELECTED_BADGE_COLOR = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
inline const ImVec4 TAG_BADGE_COLOR = ImVec4(0.25f, 0.40f, 0.65f, 1.0f);    // dark blue chip bg
inline const ImVec4 TAG_TEXT_COLOR = ImVec4(0.75f, 0.88f, 1.00f, 1.0f);     // light blue text
inline const ImVec4 WATCH_BUTTON_COLOR = ImVec4(0.65f, 0.10f, 0.10f, 1.0f); // YouTube red
inline const ImVec4 WATCH_BUTTON_HOVER = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
inline const ImVec4 NO_SELECTION_COLOR = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
} // namespace PackBrowserUI

} // namespace UI