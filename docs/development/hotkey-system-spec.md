# Hotkey System Technical Specification

## Overview
This document specifies the event-driven (non-polling) hotkey capture system implemented in SuiteSpot. It avoids using per-frame loops or `IsKeyPressed` polling for discovery, instead hooking directly into the Unreal Engine 3 input pipeline via BakkesMod.

## Core Components

### 1. The Hook
- **Target:** `Function TAGame.GameViewportClient_TA.HandleKeyPress`
- **Behavior:** This function is the engine-level bottleneck for all key/button transitions. It fires immediately when a physical state change occurs.

### 2. Parameter Structure
Based on reverse-engineered symbol dumps and Unreal Engine 3 standards, the parameters for this hook are mapped using the following struct:

```cpp
struct HandleKeyPressParams {
    int ControllerId;        // 0 for primary player
    int KeyIndex;            // Global Unreal FName index
    int KeyNumber;           // FName instance number (usually 0)
    unsigned char EventType; // 0 = IE_Pressed, 1 = IE_Released, 2 = IE_Repeat, 3 = IE_DoubleClick
    unsigned char Padding[3];
    float AmountDepressed;   // 1.0 for digital, 0.0-1.0 for analog
    unsigned int bGamepad;   // 1 if controller, 0 if keyboard/mouse
};
```

### 3. Key Name Resolution
Instead of a manual lookup table, the system uses the engine's internal string table:
`std::string keyName = gameWrapper->GetFNameByIndex(p->KeyIndex);`
This returns the exact UE3 string names (e.g., `XboxTypeS_A`, `LeftShift`, `Escape`) required for BakkesMod's `setBind` system.

### 4. Logic Flow
1. **Passive Hook:** The hook is always active. It always updates `heldKeys` (insert on press, erase on release) regardless of UI state.
2. **Capture Initialization:** When a user clicks ● in the UI, `captureRow` and `captureSlot` are set.
3. **Event Filtering:** The capture UI logic only processes events where `EventType == 0` (Pressed) and `captureRow >= 0`.
4. **Special Keys:** `"Escape"` is hardcoded to cancel capture and reset `captureRow = -1`.
5. **CVar Mapping:** Valid key names are mapped to the target CVar (defined in `HOTKEY_ROWS`) and updated via `UI::Helpers::SetCVarSafely`.

### 5. Combo Hotkey Management
- **Trigger Key (Key 1):** Handled via BakkesMod's native `setBind` system. When the CVar changes, the old bind is cleared (`setBind(oldValue, "")`) and the new key is bound to the notifier command (`setBind(newValue, "ss_xxx")`).
- **Combo Key (Key 2):** A held modifier — **never bound via `setBind`**. Its CVar value is stored and checked at notifier execution time via `heldKeys.count(key2)`.
- **`heldKeys` set:** A `std::set<std::string>` in `SuiteSpot`. The `HandleKeyPress` hook always runs (not gated on `captureRow`) and inserts the key name on `IE_Pressed` (EventType 0) and erases it on `IE_Released` (EventType 1). This is event-driven, not polled.
- **Notifier combo check:** When the notifier fires (via key1's setBind), it reads `key2` from `settingsSync`. If key2 is non-empty and **not** in `heldKeys`, the notifier returns early — action does not fire.
- **Why not `IsKeyPressed`:** `VK_GAMEPAD_*` codes (0xC3–0xD2) are UWP-only; `GetAsyncKeyState` (used internally) ignores them in Win32/BakkesMod context and always returns false for Xbox controller buttons.

**Rules to maintain:**
| Slot | setBind? | heldKeys tracked? |
|------|----------|-------------------|
| key1 | ✅ Set on save, cleared on change | Passively (but not used for gating) |
| key2 | ❌ Never — store value only | ✅ Used to gate notifier execution |

## Data Structures
Shared between `SuiteSpot.cpp` and `SettingsUI.cpp` via `ConstantsUI.h`:

```cpp
struct HotkeyRow {
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
```
