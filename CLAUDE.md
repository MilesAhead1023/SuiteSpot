# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

SuiteSpot is a BakkesMod plugin for Rocket League that automatically loads training content (training packs, freeplay maps, or workshop maps) after matches end. It's a C++20 Windows x64 DLL that integrates with BakkesMod's SDK.

## Build System — Two Separate Pipelines

There are two completely independent build pipelines. They must never be mixed.

| | Local build | CI build (GitHub Actions) |
|---|---|---|
| **Where** | Developer machine | GitHub runner (`D:\a\...`) |
| **Trigger** | Manual | Push / PR to any branch |
| **BakkesMod SDK** | `%AppData%\bakkesmod\bakkesmod\bakkesmodsdk` via registry | Cloned into `bakkesmodsdk/` at build time |
| **vcpkg** | `%VCPKG_ROOT%` env var (local install) | `C:\vcpkg` (pre-installed on runner) |
| **Post-build** | Hot-reloads plugin into live BakkesMod | No — artifact uploaded only |
| **Intermediates** | `build\.intermediates\` | Same, but discarded after run |
| **Output** | `plugins\SuiteSpot.dll` → copied to `%AppData%\bakkesmod` | Uploaded as GitHub Actions artifact |

**Key isolation rules:**
- `bakkesmodsdk/` is gitignored — CI clones it fresh, local uses the registry path
- `vcpkg_installed/` is gitignored — each environment manages its own package cache
- `build/` intermediates are gitignored — never shared between environments
- `plugins/*.dll` is gitignored — local output never committed

### Local build (from Windows PowerShell)
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

**CI build (clones BakkesMod SDK):**
```bash
git clone --depth 1 https://github.com/bakkesmodorg/BakkesModSDK.git bakkesmodsdk
cd bakkesmodsdk && git checkout 479e8f571cf554b25f4eeb64d611dec4133edcaf && cd ..
msbuild /m /p:Configuration=Release /p:BakkesModPath="." SuiteSpot.sln
```

For local development, the BakkesMod SDK path is configured in `BakkesMod.props`. The post-build step copies the DLL and resources to the BakkesMod plugins folder and patches it with `bakkesmod-patch.exe`. If Rocket League is running, the post-build hot-reloads the plugin (unload → copy → load). **Hot-reload crashes** can occur if the plugin modifies shared resources (font atlas, textures) during reload — always use `gameWrapper->Execute()` to defer such operations.

## Architecture

The project uses a **Hub-and-Spoke** pattern where `src/SuiteSpot.cpp` (the Hub class) orchestrates all functionality:

```
src/SuiteSpot.cpp (Hub)
├── src/core/AutoLoadFeature     - Post-match automation (Freeplay/Training/Workshop)
├── src/core/SettingsSync        - CVar-backed settings management
├── src/core/MapManager          - Workshop map discovery and paths
├── src/core/TrainingPackManager - 2300+ pack database with search/filter
├── src/core/WorkshopDownloader  - RLMAPS API integration for downloads
├── src/core/LoadoutManager      - Car preset management
├── src/core/PackUsageTracker    - Usage statistics for favorites
├── src/core/TextureDownloader   - Auto-downloads 14 workshop editor textures
└── UI Components
    ├── src/ui/SettingsUI        - F2 menu (tabs: Map Select, Loadout, Workshop)
    ├── src/ui/TrainingPackUI    - Floating pack browser window (PluginWindow)
    ├── src/ui/LoadoutUI         - Car preset selection panel
    └── src/ui/StatusMessageUI  - Toast notifications (Timer/Fade/ManualDismiss)
```

Components don't communicate directly—all coordination flows through the Hub.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/SuiteSpot.cpp/.h` | Plugin entry point, lifecycle management, event routing |
| `src/core/AutoLoadFeature.cpp/.h` | Core match-end automation with delay scheduling |
| `src/core/SettingsSync.cpp/.h` | CVars (prefixed `suitespot_*`) with change callbacks |
| `src/core/TrainingPackManager.cpp/.h` | JSON database operations, search, filtering |
| `src/core/WorkshopDownloader.cpp/.h` | HTTP requests to RLMAPS API, thread-safe downloads |
| `src/core/TextureDownloader.cpp/.h` | Background download of 14 workshop editor textures |
| `src/core/PackUsageTracker.cpp/.h` | Load counts + timestamps, powers favorites ranking |
| `src/core/LoadoutManager.cpp/.h` | Car preset switching via game-thread Execute() |
| `src/core/MapList.h` | Data structures: `MapEntry`, `TrainingEntry`, `WorkshopEntry` |
| `src/core/DefaultPacks.h` | Curated "Flicks Picks" default training pack list |
| `src/ui/SettingsUI.cpp/.h` | F2 settings menu (Map Select, Loadout, Workshop tabs) |
| `src/ui/TrainingPackUI.cpp/.h` | Floating pack browser PluginWindow |
| `src/ui/LoadoutUI.cpp/.h` | Car preset selection panel |
| `src/ui/StatusMessageUI.cpp/.h` | Reusable toast notification system |
| `src/ui/ConstantsUI.h` | All UI styling constants (colors, sizes, spacing) |
| `src/ui/HelpersUI.cpp/.h` | ImGui helper widgets (InputIntWithRange, ComboWithTooltip, etc.) |
| `src/utils/logging.h` | spdlog-based logging macros |
| `src/utils/ProcessUtils.h` | Process/path utilities |

## Technical Patterns

**Thread Safety:** WorkshopDownloader and TrainingPackManager use mutexes. Downloads run in background threads to avoid blocking the game. WorkshopDownloader uses `weak_ptr` + generation tracking to safely handle async HTTP callbacks — always check `searchGeneration` matches before touching shared state.

**Game Thread Safety:** Use `gameWrapper->Execute()` to defer operations to the game thread between frames. Use `gameWrapper->SetTimeout()` for delayed execution. Both patterns prevent crashes from modifying shared state (font atlas, game wrappers) during rendering. See `LoadoutManager.cpp` for the canonical `Execute()` pattern.

**Delayed Execution:** Uses `gameWrapper->SetTimeout()` to schedule commands after match-end. Minimum 0.1s delay prevents crashes during game state transitions.

**Settings Persistence:** All settings use BakkesMod CVars with `.addOnValueChanged()` callbacks for immediate sync. CVars auto-persist to `config.cfg`.

**UI Framework:** ImGui 1.75 with DirectX 11 backend. Vendor + extension widgets in `external/imgui/` (range sliders, searchable combos, timeline).

### Font Loading (CRITICAL — crashes if done wrong)

BakkesMod provides `GUIManager::LoadFont()` to load fonts into the ImGui atlas. **`LoadFont` triggers an atlas rebuild that will crash the game if called during rendering or hot-reload.**

**Safe pattern (used in `SuiteSpot.cpp` SetImGuiContext):**
```cpp
// 1. Try GetFont first — font survives hot-reload in the atlas
clockFont = gui.GetFont("suitespot_clock_48");
// 2. Only LoadFont on cold start, deferred to game thread via Execute()
if (!clockFont) {
    gameWrapper->Execute([this](GameWrapper* gw) {
        auto gui = gw->GetGUIManager();
        auto [res, font] = gui.LoadFont("name", "font.ttf", 48);
        if (res == 2 && font) clockFont = font;
    });
}
```

**Rules:**
- NEVER call `LoadFont` inside a render function
- NEVER call `LoadFont` directly in `SetImGuiContext` — wrap in `Execute()`
- Always try `GetFont` first — it returns the existing font without rebuilding
- Font files live in `bakkesmod/data/fonts/` (currently: `Ubuntu-Regular.ttf`)
- `LoadFont` returns: 0=failed, 1=queued, 2=loaded
- Use `GetFont` in the render path to lazily resolve async-loaded fonts

### ImGui Layout Tips

- **Overlay text without affecting layout:** Use `ImDrawList::AddText(font, fontSize, pos, color, text)` — draws directly, doesn't touch ImGui's cursor or layout flow.
- **DO NOT** use `SetCursorPos` to move the cursor backward for overlapping elements — it breaks layout for all subsequent items.
- **Font scaling produces blurry text.** Both `SetWindowFontScale` and `AddText` with scaled `fontSize` stretch the bitmap atlas. For crisp large text, load a font at the target pixel size via `LoadFont`.
- `GetItemRectMin()`/`GetItemRectSize()` after `EndGroup()` gives the group's bounding box for positioning overlays.

## Data Locations

```
%APPDATA%\bakkesmod\bakkesmod\
├── plugins\SuiteSpot.dll
├── data\
│   ├── SuiteSpot\TrainingSuite\
│   │   ├── training_packs.json    (2300+ packs, 2.6MB)
│   │   └── pack_usage_stats.json  (user history)
│   └── fonts\Ubuntu-Regular.ttf   (clock display font, 48px)
└── cfg\config.cfg                 (CVars/settings)
```

## External Dependencies

- **BakkesMod SDK:** Game hooks, CVars, ImGui integration
- **nlohmann/json:** JSON parsing (`json.hpp`)
- **RLMAPS API:** `https://celab.jetfox.ovh/api/v4/projects/` for workshop map search/download
- **PowerShell:** Used for ZIP extraction and training pack database updates

## CVar Naming Convention

All CVars use `suitespot_` prefix: `suitespot_enabled`, `suitespot_map_type`, `suitespot_delay_queue`, etc.

## Related Projects

**BakkesMod RAG Documentation System:** For a Python-based RAG system to query BakkesMod SDK documentation, see the separate repository at [github.com/MilesAhead1023/BakkesMod-RAG-Documentation](https://github.com/MilesAhead1023/BakkesMod-RAG-Documentation). This tool was originally developed to assist with SuiteSpot development but is now maintained independently.
