# SuiteSpot

> **Rocket League training automation for BakkasMod** — automatically loads your preferred training content (freeplay, training packs, or workshop maps) the moment a match ends.

[![Build](https://github.com/MilesAhead1023/SuiteSpot/actions/workflows/msbuild.yml/badge.svg)](https://github.com/MilesAhead1023/SuiteSpot/actions/workflows/msbuild.yml)
[![Version](https://img.shields.io/badge/version-1.0.0-blue)](#)
[![BakkasMod](https://img.shields.io/badge/requires-BakkasMod-orange)](https://bakkesmod.com)

---

## What is SuiteSpot?

SuiteSpot is a [BakkasMod](https://bakkesmod.com) plugin for Rocket League that eliminates the manual steps between finishing a match and starting your next training session. The moment a match ends, SuiteSpot loads your configured training content automatically — no lobby navigation, no menus.

Beyond auto-loading, SuiteSpot is a full training management suite: a searchable browser for 2300+ community training packs, a workshop map downloader, loadout management, usage-driven favorites, and configurable hotkeys for everything.

---

## Features

### Core Automation
- **Post-Match Auto-Load** — Automatically loads Freeplay, a Training Pack, or a Workshop Map when any match ends
- **Auto-Queue** — Optionally queues for a new match after training with a configurable delay
- **Per-Mode Delays** — Configure how long to wait before loading for each mode (prevents crash-on-transition)

### Training Pack Browser
- **2300+ Community Packs** — Full database with name, creator, difficulty, tags, and shot count
- **Advanced Filters** — Search by name/creator/code, filter by difficulty, tags, shot count range, or video-only packs
- **Sortable Columns** — Sort by name, creator, difficulty, or shot count (ascending/descending)
- **Virtual Scrolling** — Smooth performance even with the full 2300+ pack list visible
- **YouTube Thumbnails** — Pack preview images lazy-loaded on demand
- **Custom Packs** — Add, edit, and delete your own packs with full metadata

### Quick Picks
- **Flicks Picks** — Curated list of high-quality community packs, ready to use out of the box
- **Your Favorites** — Automatically ranked by your actual usage history
- **Configurable Count** — Choose how many quick picks to cycle through

### Workshop Maps
- **RLMAPS Downloader** — Search and download workshop maps directly from within BakkasMod
- **Local Browser** — Two-panel browsing of your installed workshop maps with preview images
- **Auto-Extract** — Downloads ZIP, extracts, renames `.udk`→`.upk`, creates metadata automatically

### Hotkeys
- **5 Configurable Actions** — Cycle map mode forward/back, cycle map forward/back, load now
- **Dual-Key Combos** — Each action requires a trigger key + a held modifier (prevents accidental triggers)

### Quality of Life
- **Loadout Manager** — Switch car presets during training sessions
- **Pack Healer** — Automatically fixes incorrect shot counts in the training pack database
- **Training Game Speed Fix** — Prevents custom game speed from being reset after loading a training pack
- **Texture Auto-Installer** — Downloads 14 required workshop editor textures automatically on first run
- **Usage Tracking** — Tracks every pack load with timestamps to power the Favorites ranking

---

## Requirements

- **Rocket League** (Steam or Epic Games)
- **[BakkasMod](https://bakkesmod.com)** (latest version)
- **[WorkshopMapLoader](https://bakkesmod.com/view/plugin/24)** — Required for workshop map browsing (optional if you don't use workshop maps)
- **Windows 10/11 x64**

---

## Installation

### Via BakkasMod Plugin Manager (Recommended)

1. Open Rocket League with BakkasMod running
2. Press **F2** to open BakkasMod settings
3. Go to **Plugin Manager**
4. Search for **SuiteSpot** and click **Install**

### Manual Install

1. Download `SuiteSpot.dll` from the [latest release](https://github.com/MilesAhead1023/SuiteSpot/releases/latest)
2. Copy `SuiteSpot.dll` to `%AppData%\bakkesmod\bakkesmod\plugins\`
3. Download `training_packs.json` from the same release
4. Create the folder `%AppData%\bakkesmod\bakkesmod\data\SuiteSpot\TrainingSuite\`
5. Copy `training_packs.json` to that folder
6. Copy the `fonts/` contents to `%AppData%\bakkesmod\bakkesmod\data\fonts\`
7. Restart Rocket League / reload BakkasMod

---

## Quick Start

1. **Enable the plugin** — Open F2 Settings → SuiteSpot → check **Enable Auto-Load**
2. **Choose your mode** — Select Freeplay, Training Pack, or Workshop Map
3. **Pick your content** — Select a freeplay map, training pack, or workshop map from the dropdowns
4. **Set a delay** (optional) — Set 1-3 seconds if you experience any instability on load
5. **Finish a match** — SuiteSpot loads your content automatically when the match ends

---

## Configuration

All settings use the `suitespot_` CVar prefix and persist automatically to `config.cfg`.

### Core Settings

| CVar | Type | Default | Description |
|------|------|---------|-------------|
| `suitespot_enabled` | bool | `false` | Master on/off switch |
| `suitespot_map_type` | int | `0` | Load mode: `0`=Freeplay, `1`=Training Pack, `2`=Workshop Map |
| `suitespot_auto_queue` | bool | `false` | Queue for a new match after loading training |
| `suitespot_delay_queue_sec` | int | `0` | Seconds to wait before queuing |
| `suitespot_delay_freeplay_sec` | int | `0` | Seconds to wait before loading freeplay |
| `suitespot_delay_training_sec` | int | `0` | Seconds to wait before loading training pack |
| `suitespot_delay_workshop_sec` | int | `0` | Seconds to wait before loading workshop map |

### Map Selection

| CVar | Type | Description |
|------|------|-------------|
| `suitespot_current_freeplay_code` | string | BakkasMod freeplay map code (e.g., `beckwith_park_p`) |
| `suitespot_current_training_code` | string | Training pack share code (e.g., `XXXX-XXXX-XXXX-XXXX`) |
| `suitespot_current_workshop_path` | string | Full path to workshop map `.udk` file |

### Quick Picks

| CVar | Type | Default | Description |
|------|------|---------|-------------|
| `suitespot_quick_picks_list_type` | int | `0` | `0`=Flicks Picks, `1`=Your Favorites |
| `suitespot_quick_picks_count` | int | `10` | Number of packs to cycle through |
| `suitespot_quick_picks_selected` | string | | Currently selected quick pick code |

### Misc Features

| CVar | Type | Default | Description |
|------|------|---------|-------------|
| `suitespot_training_game_speed_fix` | bool | `true` | Re-apply game speed CVar after loading a training pack |

---

## Hotkeys

Hotkeys use dual-key combos — you must hold **Key 2** and then press **Key 1** to trigger the action.

### Configuring Hotkeys

1. Open F2 Settings → SuiteSpot → **Hotkeys** tab
2. Click the capture button next to the action you want
3. Press the key you want as **Key 1** (trigger), then the key you want as **Key 2** (held modifier)

### Available Actions

| Action | Description |
|--------|-------------|
| Cycle Mode Forward | Switch to next load mode (Freeplay → Training → Workshop → Freeplay) |
| Cycle Mode Backward | Switch to previous load mode |
| Cycle Map Forward | Move to next map/pack in your current selection |
| Cycle Map Backward | Move to previous map/pack |
| Load Now | Immediately load the currently configured content |

### Hotkey CVars

| CVar | Description |
|------|-------------|
| `suitespot_hotkey_mode_fwd_key1` | Trigger key for Cycle Mode Forward |
| `suitespot_hotkey_mode_fwd_key2` | Held modifier for Cycle Mode Forward |
| `suitespot_hotkey_mode_bk_key1` | Trigger key for Cycle Mode Backward |
| `suitespot_hotkey_mode_bk_key2` | Held modifier for Cycle Mode Backward |
| `suitespot_hotkey_cycle_map_fwd_key1` | Trigger key for Cycle Map Forward |
| `suitespot_hotkey_cycle_map_fwd_key2` | Held modifier for Cycle Map Forward |
| `suitespot_hotkey_cycle_map_bk_key1` | Trigger key for Cycle Map Backward |
| `suitespot_hotkey_cycle_map_bk_key2` | Held modifier for Cycle Map Backward |
| `suitespot_hotkey_load_now_key1` | Trigger key for Load Now |
| `suitespot_hotkey_load_now_key2` | Held modifier for Load Now |

---

## Training Pack Browser

Open the browser from the F2 Settings → SuiteSpot → **Training Packs** tab, or via the floating browser window.

### Search and Filters

- **Search bar** — Matches against pack name, creator name, and pack code simultaneously
- **Difficulty** — Filter by: Unranked, Bronze, Silver, Gold, Platinum, Diamond, Champion, Grand Champion, Supersonic Legend
- **Tags** — Filter by mechanic type (Air Dribble, Defense, Dribble, Flick, Ground Shot, etc.)
- **Shot Range** — Drag the range slider to filter by number of training shots
- **Video Only** — Show only packs that have a YouTube preview video

### Actions

- **Load** — Immediately loads the selected pack
- **Set as Active** — Sets the pack as the current training pack for auto-load
- **Favorite** — Adds to your usage history (counts toward Your Favorites)
- **Add Pack** — Opens the custom pack modal to add your own pack manually

---

## Workshop Maps

### Downloading Maps

1. Open F2 Settings → SuiteSpot → **Workshop** tab
2. Type in the search box to search the RLMAPS catalog
3. Click a map to see its preview image and available releases
4. Click **Download** next to a release
5. The map is automatically downloaded, extracted, and added to your local browser

### Browsing Installed Maps

1. The left panel shows all maps in your configured workshop directory
2. Click a map to preview it in the right panel
3. Click **Load** to load the selected map immediately

### Configuring the Workshop Path

SuiteSpot reads the workshop path from the [WorkshopMapLoader](https://bakkesmod.com/view/plugin/24) plugin. Install and configure that plugin first, then SuiteSpot will automatically find your maps.

---

## Architecture

SuiteSpot uses a **Hub-and-Spoke** pattern — `src/SuiteSpot.cpp` is the Hub that orchestrates all components. Components never communicate with each other directly; everything flows through the Hub.

```
src/SuiteSpot.cpp (Hub)
├── src/core/AutoLoadFeature     Post-match automation
├── src/core/TrainingPackManager 2300+ pack database
├── src/core/WorkshopDownloader  RLMAPS API + downloads
├── src/core/SettingsSync        CVar management
├── src/core/LoadoutManager      Car preset switching
├── src/core/PackUsageTracker    Favorites ranking
├── src/core/TextureDownloader   Workshop texture install
└── UI Components
    ├── src/ui/SettingsUI        F2 menu
    ├── src/ui/TrainingPackUI    Pack browser
    ├── src/ui/LoadoutUI         Loadout panel
    └── src/ui/StatusMessageUI  Toast notifications
```

See [`docs/architecture.md`](docs/architecture.md) for the full technical reference.

---

## Building from Source

### Requirements

- **Visual Studio 2022** (v143 toolset for Release, v145 for optimized builds)
- **BakkasMod SDK** — installed via BakkasMod (registry path auto-detected)
- **vcpkg** — set `VCPKG_ROOT` environment variable to your vcpkg installation

### Build Command (PowerShell)

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Or via the batch script:

```powershell
.\build.bat
```

The DLL is output to `plugins\SuiteSpot.dll`. If BakkasMod is running, the post-build step automatically hot-reloads the plugin.

### Dependencies (vcpkg manifest)

vcpkg packages are declared in `vcpkg.json` and installed automatically by MSBuild:
- `sqlite3`, `spdlog`, `fmt`, `nlohmann-json`, `openssl`, `ixwebsocket`

### CI Build

Every push triggers a GitHub Actions build (`.github/workflows/msbuild.yml`) that:
1. Clones the BakkasMod SDK fresh
2. Restores vcpkg packages from cache
3. Builds Release|x64
4. Uploads `SuiteSpot.dll` as a build artifact

---

## Contributing

### Conventions

- **CVar prefix:** All CVars must use `suitespot_` (e.g., `suitespot_my_new_setting`)
- **Naming:** Classes `CamelCase`, functions `CamelCase`, variables `camelCase`, constants `UPPER_CASE`
- **Code style:** `.clang-format` and `.clang-tidy` are enforced — run before submitting
- **Comments:** Only for non-obvious logic; self-documenting code preferred

### Game Thread Safety

Any code that modifies shared state (game objects, ImGui font atlas) must run on the game thread:

```cpp
gameWrapper->Execute([this](GameWrapper* gw) {
    // safe to modify shared state here
});
```

See [`docs/architecture.md`](docs/architecture.md) for full threading and font loading patterns.

### Running Tests

Tests live in `tests/` and use [Catch2](https://github.com/catchorg/Catch2). They run on Linux/WSL2 without a live game:

```bash
cd tests && make && ./suitespot_tests
```

### Submitting Changes

1. Fork the repository
2. Create a branch: `git checkout -b feature/my-feature`
3. Make changes and run the build: `.\build.bat`
4. Run tests if applicable: `cd tests && make && ./suitespot_tests`
5. Commit with a conventional commit message: `feat: add my feature`
6. Open a pull request

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Acknowledgments

- [BakkasMod](https://bakkesmod.com) — the plugin framework that makes this possible
- [RLMAPS / RLMaps.gg](https://rlmaps.gg) — workshop map API
- The Rocket League community pack creators whose content powers the training database
