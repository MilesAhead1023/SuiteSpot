# SuiteSpot — Copilot Instructions

SuiteSpot is a BakkesMod plugin for Rocket League (C++20, Windows x64 DLL) that automatically loads training content after matches end. It uses a **Hub-and-Spoke architecture** orchestrated by `src/SuiteSpot.cpp`, with components communicating exclusively through the Hub to avoid tight coupling.

## Build & Test Commands

**Build (Windows PowerShell, release config):**
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

**Or via batch script:**
```powershell
.\build.bat
```

**Post-build:** If BakkesMod + Rocket League are running, the DLL is automatically hot-reloaded into the live plugin (no manual restart needed).

**Run tests (Linux/WSL2, requires Catch2):**
```bash
cd tests && make && ./suitespot_tests
```

**Run single test file:**
```bash
cd tests && make test_training_pack_manager && ./suitespot_tests -t "test_training_pack_manager"
```

**CI build** (GitHub Actions, for reference): Clones BakkesMod SDK fresh, caches vcpkg packages, uploads artifact.

## High-Level Architecture

```
src/SuiteSpot.cpp (Hub) — Event dispatcher & lifecycle
├── src/core/AutoLoadFeature     Post-match automation with delay scheduling
├── src/core/SettingsSync        BakkesMod CVar management (suitespot_* prefix)
├── src/core/MapManager          Workshop map discovery
├── src/core/TrainingPackManager 2300+ pack database (JSON), search/filter
├── src/core/WorkshopDownloader  RLMAPS API integration, async downloads
├── src/core/LoadoutManager      Car preset management
├── src/core/PackUsageTracker    Usage stats, favorites, recency scoring
├── src/core/TextureDownloader   Auto-downloads 14 workshop editor textures
└── UI Components
    ├── src/ui/SettingsUI        F2 menu (tabs: Map Select, Loadout, Workshop)
    ├── src/ui/TrainingPackUI    Floating pack browser window (PluginWindow)
    ├── src/ui/LoadoutUI         Car preset selection
    └── src/ui/StatusMessageUI  Toast notifications (Timer/Fade/ManualDismiss)
```

**Key rule:** Components do NOT communicate directly—all coordination flows through `SuiteSpot` (the Hub). This prevents circular dependencies and makes threading safe.

## Build System: Two Pipelines

Local and CI builds use **different SDK/vcpkg paths** and must never mix.

| Aspect | Local | CI |
|--------|-------|-----|
| SDK | `%AppData%\bakkesmod\bakkesmodsdk` (via registry) | Cloned to `bakkesmodsdk/` |
| vcpkg | `%VCPKG_ROOT%` (local install) | `C:\vcpkg` (pre-installed) |
| Post-build | Hot-reload into live BakkesMod | DLL uploaded as artifact |
| Intermediates | `build\.intermediates\` | Discarded after run |

**Gitignored paths:** `bakkesmodsdk/`, `vcpkg_installed/`, `build/`, `plugins/*.dll` — these are environment-specific and must never be committed.

## Critical Patterns

### Game Thread Safety (CRASHES if violated)

Use `gameWrapper->Execute()` to defer operations to the game thread between frames. This prevents crashes from modifying shared state (font atlas, game wrappers) during rendering.

**Pattern (see `src/core/LoadoutManager.cpp` for canonical example):**
```cpp
gameWrapper->Execute([this](GameWrapper* gw) {
    // Safe to modify shared state here, runs between game frames
    auto gui = gw->GetGUIManager();
    // ... perform UI operations
});
```

Use `gameWrapper->SetTimeout()` for delayed execution after match-end (minimum 0.1s delay prevents crashes during state transitions).

### Font Loading (Game Crash Risk)

**NEVER** call `GUIManager::LoadFont()` inside render functions or directly in `SetImGuiContext()`. The atlas rebuild crashes the game during rendering.

**Safe pattern (from `src/SuiteSpot.cpp`):**
```cpp
// 1. Try GetFont first — survives hot-reload in atlas
clockFont = gui.GetFont("suitespot_clock_48");
// 2. LoadFont only on cold start, wrapped in Execute()
if (!clockFont) {
    gameWrapper->Execute([this](GameWrapper* gw) {
        auto gui = gw->GetGUIManager();
        auto [res, font] = gui.LoadFont("name", "font.ttf", 48);
        if (res == 2 && font) clockFont = font;
    });
}
```

### Thread Safety (Downloads, JSON)

`WorkshopDownloader` and `TrainingPackManager` use mutexes for thread-safe access. `WorkshopDownloader` uses `weak_ptr` + generation tracking to safely cancel async HTTP callbacks—always check `searchGeneration` matches before touching shared state.

### ImGui Layout Tips

- **Overlay text without affecting layout:** Use `ImDrawList::AddText(font, fontSize, pos, color, text)` — draws directly without touching cursor.
- **DO NOT** use `SetCursorPos` to move cursor backward for overlapping elements—it breaks layout for all subsequent items.
- **Font scaling produces blurry text.** Load fonts at target pixel size via `LoadFont`, don't scale after.
- `GetItemRectMin()`/`GetItemRectSize()` after `EndGroup()` gives bounding box for positioning overlays.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/SuiteSpot.cpp/.h` | Hub: plugin entry, lifecycle, event routing |
| `src/core/AutoLoadFeature.cpp/.h` | Core: post-match automation, delay scheduling |
| `src/core/SettingsSync.cpp/.h` | CVar management (all `suitespot_*` prefix) |
| `src/core/TrainingPackManager.cpp/.h` | JSON database ops, search, filtering, pagination |
| `src/core/WorkshopDownloader.cpp/.h` | HTTP requests (RLMAPS API), thread-safe downloads |
| `src/core/TextureDownloader.cpp/.h` | Background download of 14 workshop editor textures |
| `src/core/PackUsageTracker.cpp/.h` | Load counts + timestamps, powers favorites ranking |
| `src/core/LoadoutManager.cpp/.h` | Car preset switching via game-thread Execute() |
| `src/core/MapList.h` | Data structures: `MapEntry`, `TrainingEntry`, `WorkshopEntry` |
| `src/core/DefaultPacks.h` | Curated "Flicks Picks" default training pack list |
| `src/ui/SettingsUI.cpp/.h` | F2 settings menu (Map Select, Loadout, Workshop tabs) |
| `src/ui/TrainingPackUI.cpp/.h` | Floating pack browser PluginWindow |
| `src/ui/ConstantsUI.h` | All UI styling constants (colors, sizes, spacing) |
| `src/ui/HelpersUI.cpp/.h` | ImGui helper widgets (InputIntWithRange, ComboWithTooltip, etc.) |
| `src/utils/logging.h` | spdlog-based logging macros |

## Dependencies

- **BakkesMod SDK:** Game hooks, CVars, ImGui integration (Windows x64, header-only)
- **vcpkg packages:** sqlite3, spdlog, fmt, nlohmann-json, openssl, ixwebsocket
- **RLMAPS API:** `https://celab.jetfox.ovh/api/v4/projects/` for workshop downloads
- **ImGui:** 1.75 with DirectX 11, custom widgets in `IMGUI/` folder

## Testing

Testable components (no live game required):
- `TrainingPackManager` — JSON loading, search, filtering
- `SettingsSync` — CVar defaults, validation, range clamping
- `PackUsageTracker` — Usage stats, favorites, recency
- `MapManager` — Path resolution, discovery

**Add a test:** Create `tests/test_*.cpp` with `#include "catch.hpp"`. Makefile auto-discovers and compiles.

## Conventions

- **CVar prefix:** All CVars use `suitespot_` (e.g., `suitespot_enabled`, `suitespot_delay_queue`)
- **Naming (clang-tidy enforced):** Classes `CamelCase`, functions `CamelCase`, variables `camelCase`, constants `UPPER_CASE`
- **Code style:** `.clang-format` enforces formatting; `.clang-tidy` enforces correctness (thread safety, modern C++20, pointer safety)
- **Comments:** Only clarify non-obvious logic; self-documenting code preferred

## Data Paths

```
%APPDATA%\bakkesmod\bakkesmod\
├── plugins\SuiteSpot.dll
├── data\SuiteSpot\TrainingSuite\
│   ├── training_packs.json    (2300+ packs, 2.6MB)
│   └── pack_usage_stats.json  (user history)
└── cfg\config.cfg             (CVars persist here)
```
