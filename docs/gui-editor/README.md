# SuiteSpot GUI Replica (External Editor)

This folder contains a code-driven external recreation of the in-game SuiteSpot UI so you can edit/iterate outside Rocket League.

## Open it
- Open `docs/gui-editor/index.html` in a browser.

## What matches
- Main settings frame and BakkesMod-style dark layout.
- `Map Select` and `Loadout Management` tab flow from `SettingsUI.cpp` + `LoadoutUI.cpp`.
- Training quick-picks list state and workshop installed-maps state.
- Separate Training Browser overlay state (matching the standalone browser screenshot).

## Presets
Use the `Screenshot Preset` dropdown to jump between captured states:
- `freestyle_map_select`
- `training_flicks`
- `training_favorites`
- `workshop_map_select`
- `workshop_loadout`
- `training_browser_window`

## Edit workflow
- Inline-edit header text directly in the replica.
- Turn on `Design Mode` for per-layer editing in each map mode window (`freeplay` / `training` / `workshop`).
- Drag by layer title, resize from the bottom-right handle, and use arrow keys for nudging (`Shift` = bigger step).
- Use the Selected Layer inspector for precise `x/y/w/h`, visibility/lock, opacity, colors, z-order, duplicate, add, delete.
- Use Export/Import JSON to persist all UI data + layout/editor state and iterate quickly.
