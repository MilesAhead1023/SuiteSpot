# Hotkeys & Toast Notifications QOL Feature

**Date:** 2026-02-22
**Status:** Implementation Phase (Hotkey API + Toast system integrated)
**Branch:** `qol/touchups`

## Overview

Implement hotkey bindings and toast notifications to allow users to switch map modes, cycle maps, and load maps without entering the settings screen. Hotkey investigation is in progress to determine single-key vs. multi-key feasibility.

## Goals

1. **Hotkey Actions:**
   - Cycle map mode forward (Freeplay → Training → Workshop → Freeplay)
   - Cycle map mode backward (reverse order)
   - Cycle maps within current mode (next/previous available map)
   - Load current map immediately (skip queue)

2. **Toast Notifications:**
   - Display action + current selection when hotkey is pressed
   - Appear in top-left corner
   - Fade out after 7 seconds
   - Use existing `StatusMessageUI` infrastructure

3. **User Experience:**
   - Quick access to common actions without opening settings
   - Clear feedback when actions execute
   - Minimal visual clutter during gameplay

## Architecture

### Toast Notification System

**Reuse existing infrastructure:**
- `StatusMessageUI::StatusMessage` class already exists and is production-ready
- Supports multiple display modes: Timer, TimerWithFade, ManualDismiss
- Already used in LoadoutUI, SettingsUI, TrainingPackUI

**Integration points:**
1. Create a **global toast singleton** or inject into SuiteSpot hub
2. When hotkey action executes, call `toast.ShowInfo(message, 7.0f)`
3. Toast renders in top-left, auto-fades after 7 seconds

**Message format (Question 3 answer: B):**
```
"Switched to Training Mode • Flicks (34/90)"
```

- Action: "Switched to Training Mode"
- Current selection: "Flicks (34/90)" (map name + current/total)

**Toast appearance:**
- Position: Top-left corner (standard game notification location)
- Duration: 7 seconds fade-out (user specified)
- Color: Info blue (or success green for load confirmation)

### Hotkey Binding System

**Current state:** Hotkey investigation in progress
- Unknown: Does BakkesMod support multi-key (Ctrl+F, Shift+F, etc.)?
- Unknown: What's the API for registering hotkeys?

**Implementation approach (TBD):**
- **Phase 1 (Investigation):** Explore BakkesMod SDK documentation and test multi-key feasibility
- **Phase 2 (Implementation):** Implement based on findings
  - If multi-key works: Use Ctrl+key combinations (intuitive)
  - If single-key only: Use F3, F4, F5 pattern (fallback)

**Integration with existing CVars:**
The feature will reuse command CVars:
- `ss_cycle_map_mode_fwd` — cycle to next map mode
- `ss_cycle_map_mode_bk` — cycle to previous map mode
- `ss_cycle_map_fwd` — cycle to next available map
- `ss_cycle_map_bk` — cycle to previous available map
- `ss_load_now` — load current map immediately

When a hotkey is pressed:
1. BakkesMod calls the registered hotkey handler
2. Handler checks which hotkey triggered it
3. Handler executes the corresponding command CVar
4. Command CVar sets a flag: "show_toast_on_next_execute"
5. On next frame, if flag is set, toast renders with action details

### Data Flow

```
User presses hotkey
        ↓
BakkesMod hotkey handler triggered
        ↓
Call command CVar (e.g., ss_cycle_map_mode_fwd)
        ↓
CVar executes action (change map mode)
        ↓
Set flag: showToastOnHotkey = true
        ↓
Next render frame: Check flag
        ↓
Display toast with current mode/map info
        ↓
Toast fades over 7 seconds
```

### Toast Message Examples

**Cycle map mode:**
```
"Switched to Training Mode • Flicks (34/90)"
```

**Cycle maps:**
```
"Next Map • Aerial Training (75/150)"
```

**Load now:**
```
"Loading Flicks Training Pack..."
```

## Implementation Strategy

### Phase 1: Hotkey Investigation (Current)
- Research BakkesMod SDK documentation
- Test BakkesMod API for hotkey registration
- Determine if multi-key (Ctrl+F) is supported
- Document findings in investigation notes

### Phase 2: Toast Notification Integration
- Wrap `StatusMessageUI::StatusMessage` for global access
- Add `showToastOnHotkey` flag to SuiteSpot hub
- Modify command CVar handlers to set flag
- Integrate toast rendering into main render loop

### Phase 3: Hotkey Registration
- Register hotkey handlers with BakkesMod
- Implement hotkey→command mapping
- Add configuration CVars for hotkey bindings (optional, Phase 2 enhancement)

## Files to Modify/Create

| File | Purpose |
|------|---------|
| `SuiteSpot.h/.cpp` | Add toast singleton, hotkey handlers |
| `SettingsSync.cpp` | Modify CVar handlers to set toast flag |
| `SuiteSpot.cpp` (Render) | Integrate toast rendering |
| `docs/investigation/hotkey-bakkesmod-api.md` | Document findings |

## Testing Strategy

1. **Toast Notifications:**
   - [ ] Toast appears when action is triggered
   - [ ] Toast shows correct message format
   - [ ] Toast fades after 7 seconds
   - [ ] Multiple toasts don't overlap (if applicable)

2. **Hotkey Execution:**
   - [ ] Hotkey registers without errors
   - [ ] Hotkey triggers correct action
   - [ ] Toast appears only for hotkey actions (not UI clicks)
   - [ ] No input blocking issues

3. **Edge Cases:**
   - [ ] Spamming hotkey doesn't crash
   - [ ] Hotkey works during gameplay (not just menus)
   - [ ] No conflicts with other BakkesMod plugins

## Known Unknowns

- **Multi-key support:** Does BakkesMod SDK support Ctrl+key combinations?
  - If yes: Use intuitive Ctrl+F pattern
  - If no: Fall back to F3, F4, F5
- **Hotkey API:** What's the exact BakkesMod function to register hotkeys?
- **Input blocking:** Does BakkesMod allow preventing hotkey input from reaching the game?

## Future Enhancements

1. **Hotkey Configuration UI:** Settings menu to rebind hotkeys
2. **Multi-key Support:** If feasible after investigation
3. **Hotkey Profiles:** Save/load different hotkey sets
4. **Toast Positioning:** Configurable via CVar
5. **Silence Mode:** CVar to disable toasts for competitive play

## Success Criteria

- [x] Design approved (this document)
- [x] Hotkey investigation complete
- [x] Hotkey API identified: `GameWrapper::IsKeyPressed(int keyCode)` with manual multi-key via AND logic
- [x] Toast system integrated into SuiteSpot hub
- [x] CVars for hotkey bindings created
- [ ] CycleMapMode() and CycleMap() methods completed
- [ ] Hotkeys tested in-game (single and multi-key combinations)
- [ ] Toast notifications appearing and fading correctly
- [ ] No regressions to existing functionality
- [ ] Code follows SuiteSpot patterns (Hub-and-Spoke, thread safety)
