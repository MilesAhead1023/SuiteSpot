# SuiteSpot — Copilot Instructions

SuiteSpot is a BakkesMod plugin for Rocket League (C++20, Windows x64 DLL) that automatically loads training content after matches end. It uses a **Hub-and-Spoke architecture** orchestrated by `SuiteSpot.cpp`, with components communicating exclusively through the Hub to avoid tight coupling.

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
SuiteSpot.cpp (Hub) — Event dispatcher & lifecycle
├── AutoLoadFeature        Post-match automation with delay scheduling
├── SettingsSync           BakkesMod CVar management (suitespot_* prefix)
├── MapManager             Workshop map discovery
├── TrainingPackManager    2300+ pack database (JSON), search/filter
├── WorkshopDownloader     RLMAPS API integration, async downloads
├── LoadoutManager         Car preset management
├── PackUsageTracker       Usage stats, favorites, recency scoring
└── UI Components
    ├── SettingsUI         F2 menu (tabs: Map Select, Loadout, Workshop)
    ├── TrainingPackUI     Floating pack browser window
    ├── LoadoutUI          Car preset selection
    └── StatusMessageUI    Toast notifications
```

**Key rule:** Components do NOT communicate directly—all coordination flows through `SuiteSpot` (the Hub). This prevents circular dependencies and makes threading safe.

## Build System: Two Pipelines

Local and CI builds use **different SDK/vcpkg paths** and must never mix.

| Aspect | Local | CI |
|--------|-------|-----|
| SDK | `%AppData%\bakkesmod\bakkesmodsdk` (via registry) | Cloned to `bakkesmodsdk/` |
| vcpkg | `C:\Users\bmile\vcpkg` (VCPKG_ROOT env var) | `C:\vcpkg` (pre-installed) |
| Post-build | Hot-reload into live BakkesMod | DLL uploaded as artifact |
| Intermediates | `build\.intermediates\` | Discarded after run |

**Gitignored paths:** `bakkesmodsdk/`, `vcpkg_installed/`, `build/`, `plugins/*.dll` — these are environment-specific and must never be committed.

## Critical Patterns

### Game Thread Safety (CRASHES if violated)

Use `gameWrapper->Execute()` to defer operations to the game thread between frames. This prevents crashes from modifying shared state (font atlas, game wrappers) during rendering.

**Pattern (see `LoadoutManager.cpp` for canonical example):**
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

**Safe pattern (from `SuiteSpot.cpp`):**
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
| `SuiteSpot.cpp/.h` | Hub: plugin entry, lifecycle, event routing |
| `AutoLoadFeature.cpp/.h` | Core: post-match automation, delay scheduling |
| `SettingsSync.cpp/.h` | CVar management (all `suitespot_*` prefix) |
| `TrainingPackManager.cpp/.h` | JSON database ops, search, filtering, pagination |
| `WorkshopDownloader.cpp/.h` | HTTP requests (RLMAPS API), thread-safe downloads |
| `MapList.h` | Data structures: `MapEntry`, `TrainingEntry`, `WorkshopEntry` |
| `ConstantsUI.h` | All UI styling constants (colors, sizes, spacing) |

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

## User File Locations

- **Screenshots**: `C:\Users\bmile\Pictures\Screenshots` — when the user says "see recent screenshots", look here.
- **BakkesMod log**: `C:\Users\bmile\AppData\Roaming\bakkesmod\bakkesmod\bakkesmod.log`
- **Rocket League log**: `C:\Users\bmile\Documents\My Games\Rocket League\TAGame\Logs\Launch.log`
