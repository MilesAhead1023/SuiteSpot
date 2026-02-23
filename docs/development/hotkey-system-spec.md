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
1. **Passive Hook:** The hook is always active but immediately returns if `captureRow < 0` (no UI interaction).
2. **Capture Initialization:** When a user clicks ● in the UI, `captureRow` and `captureSlot` are set.
3. **Event Filtering:** The hook only processes events where `EventType == 0` (Pressed).
4. **Special Keys:** `"Escape"` is hardcoded to cancel capture and reset `captureRow = -1`.
5. **CVar Mapping:** Valid key names are mapped to the target CVar (defined in `HOTKEY_ROWS`) and updated via `UI::Helpers::SetCVarSafely`.

### 5. Combo Hotkey Management
- **Trigger Key (Key 1):** Handled via BakkesMod's native `setBind` system.
- **Combo Key (Key 2):** When the notifier for Key 1 fires, it checks the state of Key 2.
- **Current Limitation:** `gameWrapper->IsKeyPressed` has shown inconsistencies with some controller buttons. Future iterations should consider global state tracking within the `HandleKeyPress` hook (mapping Pressed/Released states to an internal set) to ensure 100% reliable combo detection without polling.

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
