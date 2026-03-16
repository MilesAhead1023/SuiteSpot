# CLAUDE.md — SuiteSpot Project Reference

## What This Project Is

SuiteSpot is a BakkesMod plugin for Rocket League. When a match ends, it automatically loads training content — a training pack, a freeplay map, or a workshop map — so the player doesn't have to navigate menus manually. It is a C++20 Windows x64 DLL, version 1.0.0.867.

---

## Build

**Local build (from PowerShell):**
```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

- Output DLL: `plugins\SuiteSpot.dll`
- Post-build auto-copies DLL + all assets to `%AppData%\bakkesmod\bakkesmod\` and patches the DLL
- If Rocket League is running, the plugin hot-reloads automatically
- **Hot-reload crash rule:** never call `LoadFont` or modify the ImGui atlas inside a render path or directly in `SetImGuiContext` — always defer via `gameWrapper->Execute()`

**vcpkg:** Manifest mode. `vcpkg.json` defines dependencies; packages install into `vcpkg_installed\x64-windows-static\` (gitignored).

**Dependencies:** `sqlite3`, `spdlog`, `fmt`, `nlohmann-json`, `openssl`, `ixwebsocket`

**CI build:** GitHub Actions — clones BakkesMod SDK fresh, runs MSBuild, uploads artifact. Never mixes with local paths.

---

## Architecture — Hub-and-Spoke

`src/SuiteSpot.cpp` is the Hub. Everything routes through it. Components never talk to each other directly.

```
SuiteSpot (Hub)
├── core/AutoLoadFeature      Post-match automation — decides what to load and when
├── core/SettingsSync         All CVars — registration, storage, callbacks
├── core/MapManager           Workshop map discovery, path resolution, hotkey cycling
├── core/TrainingPackManager  2300+ pack JSON database — load, search, filter
├── core/WorkshopDownloader   HTTP downloads from bakkesplugins.com API
├── core/LoadoutManager       Car preset switching via game thread
├── core/PackUsageTracker     Per-pack load counts + timestamps → favorites ranking
├── core/TextureDownloader    Downloads 14 .upk workshop editor textures on first run
└── ui/
    ├── SettingsUI            F2 settings tab (General, Map Selection, Loadout, Hotkeys)
    ├── TrainingPackUI        Floating pack browser window (PluginWindow, two-panel)
    ├── LoadoutUI             Car preset panel (renders inside SettingsUI)
    └── StatusMessageUI       Reusable toast notifications (Timer / TimerWithFade / ManualDismiss)
```

**SuiteSpot inherits from three base classes:**
- `BakkesModPlugin` — required plugin entry point
- `PluginSettingsWindow` (via `SettingsWindowBase`) — renders the F2 settings tab
- `PluginWindow` — manages the floating pack browser window

`SettingsUI`, `TrainingPackUI`, and `LoadoutUI` are all `friend` classes — they can access private Hub members directly.

---

## Data Structures (`src/core/MapList.h`)

Three global vectors. Defined in `MapList.cpp`, declared extern in `MapList.h`.

```cpp
struct MapEntry     { string code; string name; };
struct TrainingEntry { string code; string name; string creator; string difficulty;
                       vector<string> tags; int shotCount; string videoUrl; string gifUrl;
                       int likes; int plays; string source; bool isModified;
                       shared_ptr<ImageWrapper> thumbnailImage; };
struct WorkshopEntry { string filePath; string name; string author; string description;
                       path folder; path previewPath; shared_ptr<ImageWrapper> previewImage; };

extern vector<MapEntry>     SuiteMaps;      // Freeplay maps (statically defined)
extern vector<TrainingEntry> SuiteTraining; // ALWAYS EMPTY GLOBAL — use TrainingPackManager::GetPacks()
extern vector<WorkshopEntry> SuiteWorkshop; // Populated by MapManager::LoadWorkshopMaps()
```

**Important:** `SuiteTraining` (global) is never populated. Training packs live in `TrainingPackManager`'s private `SuiteTraining` member. Always use `trainingPackMgr->GetPacks()` to get the real list.

---

## Key Private Members of SuiteSpot (SuiteSpot.h)

```cpp
unique_ptr<MapManager>          mapManager
unique_ptr<SettingsSync>        settingsSync
unique_ptr<AutoLoadFeature>     autoLoadFeature
unique_ptr<TrainingPackManager> trainingPackMgr      // NOTE: not trainingPackManager
unique_ptr<LoadoutManager>      loadoutManager
unique_ptr<PackUsageTracker>    usageTracker
shared_ptr<WorkshopDownloader>  workshopDownloader
unique_ptr<TextureDownloader>   textureDownloader
shared_ptr<TrainingPackUI>      trainingPackUI
unique_ptr<SettingsUI>          settingsUI
unique_ptr<LoadoutUI>           loadoutUI

ImFont* clockFont   // Ubuntu-Regular.ttf at 48px — used for in-game clock display
ImFont* uiFont      // Roboto-Medium.ttf 14px + fa-solid-900.ttf icons merged in

bool isBrowserOpen
set<string> heldKeys        // Tracks currently held keys for combo hotkeys
int captureRow, captureSlot // Hotkey capture state
```

---

## Post-Match Loading Flow (AutoLoadFeature)

Called by `SuiteSpot::GameEndedEvent` → `autoLoadFeature->OnMatchEnded(...)`.

The Hub passes everything AutoLoadFeature needs as parameters: `freeplayMaps`, `trainingPacks` (from `trainingPackMgr->GetPacks()`), `workshopMaps`, `settings`, `usageTracker`.

**Decision tree:**
1. If disabled → return
2. If `mapType == 0` (Freeplay): verify `currentFreeplayCode` is in `SuiteMaps`, then `safeExecute(delay, "load_freeplay <code>")`
3. If `mapType == 1` (Training):
   - Try `QuickPicksSelectedCode` first
   - Fallback to `currentTrainingCode`
   - Fallback to first QuickPick from `usageTracker->GetTopUsedCodes(n)`
   - Final fallback to `DefaultPacks::FLICKS_PICKS[0]`
   - Calls `usageTracker->IncrementLoadCount(code)` before loading
   - Executes `"load_training <code>"`
4. If `mapType == 2` (Workshop): verify path in `SuiteWorkshop`, then `"load_workshop \"<path>\""`
5. If `autoQueue` is on: also `safeExecute(delayQueueSec, "queue")`

**`safeExecute` enforces minimum 0.1s delay** even when user sets 0s — prevents crashes during game state transitions.

---

## All CVars (`suitespot_*` prefix)

| CVar | Default | Description |
|------|---------|-------------|
| `suitespot_enabled` | 0 | Master on/off switch |
| `suitespot_map_type` | 0 | 0=Freeplay, 1=Training, 2=Workshop |
| `suitespot_auto_queue` | 0 | Auto-queue after map load |
| `suitespot_fix_training_gamespeed` | 1 | Sync game speed in training playlists |
| `suitespot_quickpicks_list_type` | 0 | 0=Flicks Picks, 1=Your Favorites |
| `suitespot_quickpicks_count` | 10 | Number of quick picks (5–15) |
| `suitespot_quickpicks_selected` | "" | Selected quick pick pack code |
| `suitespot_delay_queue_sec` | 0 | Delay before queueing (0–300s) |
| `suitespot_delay_freeplay_sec` | 0 | Delay before freeplay load (0–300s) |
| `suitespot_delay_training_sec` | 0 | Delay before training load (0–300s) |
| `suitespot_delay_workshop_sec` | 0 | Delay before workshop load (0–300s) |
| `suitespot_current_freeplay_code` | "" | Selected freeplay map code |
| `suitespot_current_training_code` | "" | Selected training pack code |
| `suitespot_current_workshop_path` | "" | Selected workshop map file path |
| `ss_training_maps` | "" | Legacy stored training maps (not persisted) |
| `suitespot_hotkey_map_mode_fwd_key` | "" | Cycle mode forward — trigger key |
| `suitespot_hotkey_map_mode_fwd_key2` | "" | Cycle mode forward — held key |
| `suitespot_hotkey_map_mode_bk_key` | "" | Cycle mode backward — trigger key |
| `suitespot_hotkey_map_mode_bk_key2` | "" | Cycle mode backward — held key |
| `suitespot_hotkey_cycle_map_fwd_key` | "" | Cycle map forward — trigger key |
| `suitespot_hotkey_cycle_map_fwd_key2` | "" | Cycle map forward — held key |
| `suitespot_hotkey_cycle_map_bk_key` | "" | Cycle map backward — trigger key |
| `suitespot_hotkey_cycle_map_bk_key2` | "" | Cycle map backward — held key |
| `suitespot_hotkey_load_now_key` | "" | Load current map immediately — trigger key |
| `suitespot_hotkey_load_now_key2` | "" | Load now — held key |

All CVars auto-persist via BakkesMod's `config.cfg`.

---

## Hotkey System

Dual-key combo required for every action. Key1 = the key you press; Key2 = the key you must be holding at the same time. Both must be non-empty or the hotkey does nothing.

Hook: `TAGame.GameViewportClient_TA.HandleKeyPress` — fires on every keypress in-game. SuiteSpot maintains a `set<string> heldKeys` to track which keys are currently held.

**5 hotkey actions:**
- **Cycle Mode Forward/Back** — rotates between Freeplay → Training → Workshop modes. Shows a toast.
- **Cycle Map Forward/Back** — advances the selection within the current mode. Shows a toast with the map name. Uses `trainingPackMgr->GetPacks()` for training (not the global).
- **Load Now** — immediately executes the current selection without waiting for a match to end.

Keys are UE3 strings (e.g., `"J"`, `"F3"`, `"LeftAlt"`, `"XboxTypeS_DPad_Up"`).

---

## Thread Safety Rules

| Component | Pattern | Notes |
|-----------|---------|-------|
| `TrainingPackManager` | `packMutex` | Protects internal `SuiteTraining` vector |
| `LoadoutManager` | `cacheMutex_` + `gameWrapper->Execute()` | All loadout ops deferred to game thread |
| `WorkshopDownloader` | `resultsMutex` + `searchGeneration` counter | Generation tracking cancels stale HTTP callbacks |
| `PackUsageTracker` | `mutex_` | Protects stats map |
| Font loading | `gameWrapper->Execute()` | Atlas rebuild must happen between game frames |
| Delayed execution | `gameWrapper->SetTimeout()` | Minimum 0.1s enforced in `safeExecute` |

**Never** modify the ImGui atlas, font data, or game wrappers from a non-game thread or during rendering.

---

## Font Loading (Crash-Sensitive)

Two fonts are loaded at plugin startup via `SetImGuiContext`:

**Clock font** (`clockFont`): `Ubuntu-Regular.ttf` at 48px, named `"suitespot_clock_48"`. Lives in `%AppData%\bakkesmod\bakkesmod\data\fonts\` — NOT in the plugin's assets folder.

**UI font** (`uiFont`): `Roboto-Medium.ttf` at 14px, named `"suitespot_roboto_14"`. Font Awesome 5 Solid icons (`fa-solid-900.ttf`) are merged into it at the same size (glyph range `0xF000–0xF8D9`). Both files are in `assets/fonts/` and deployed by post-build.

**Safe loading pattern — always follow this exactly:**
```cpp
clockFont = gui.GetFont("suitespot_clock_48"); // try atlas first (hot-reload safe)
if (!clockFont) {
    gameWrapper->Execute([this](GameWrapper* gw) {
        if (clockFont) return; // guard against duplicate Execute callbacks
        auto gui = gw->GetGUIManager();
        auto [res, font] = gui.LoadFont("suitespot_clock_48", "Ubuntu-Regular.ttf", 48);
        if (res == 2 && font) clockFont = font;
    });
}
```
- `LoadFont` returns: 0=failed, 1=queued, 2=loaded
- Always `GetFont` first — returns existing atlas font without a rebuild
- `LoadFont` triggers atlas rebuild — if called during rendering the game crashes

---

## ImGui Layout Rules

- **Overlay text without layout impact:** use `ImDrawList::AddText(font, size, pos, color, text)` — draws directly, doesn't touch ImGui cursor
- **Never use `SetCursorPos` to move backward** — breaks layout for all subsequent items
- **Font scaling (`SetWindowFontScale`) blurs text** — load fonts at the target pixel size instead
- `GetItemRectMin()` + `GetItemRectSize()` after `EndGroup()` gives bounding box for overlay positioning

---

## Log Locations (Debugging)

- **BakkesMod Logs:** `C:\Users\bmile\AppData\Roaming\bakkesmod\bakkesmod\bakkesmod.log`
- **Rocket League Logs:** `%USERPROFILE%\Documents\My Games\Rocket League\TAGame\Logs\Launch.log`

---

## Data Locations at Runtime

```
%APPDATA%\bakkesmod\bakkesmod\
├── plugins\SuiteSpot.dll
├── data\
│   ├── SuiteSpot\
│   │   ├── TrainingSuite\
│   │   │   ├── training_packs.json   (2300+ packs, 2.6MB, deployed by post-build)
│   │   │   └── pack_usage_stats.json (per-user load history, created at runtime)
│   │   ├── Workshop\
│   │   │   └── NoPreview.jpg         (fallback image for maps without preview)
│   │   └── Resources\
│   │       └── Icons\icon_youtube.png
│   └── fonts\
│       ├── Ubuntu-Regular.ttf        (clock font — must exist or clock won't load)
│       └── (Roboto-Medium.ttf + fa-solid-900.ttf deployed here by post-build)
└── cfg\config.cfg                    (all CVars persisted here automatically)
```

---

## Key Source Files

| File | Role |
|------|------|
| `src/SuiteSpot.cpp/.h` | Hub: lifecycle, event routing, hotkey handling, font loading |
| `src/core/AutoLoadFeature.cpp/.h` | Post-match decision engine — what to load, when |
| `src/core/SettingsSync.cpp/.h` | All CVar registration and callback wiring |
| `src/core/MapManager.cpp/.h` | Workshop discovery, path resolution, hotkey index cycling |
| `src/core/MapList.h` | `MapEntry`, `TrainingEntry`, `WorkshopEntry` structs + extern globals |
| `src/core/TrainingPackManager.cpp/.h` | JSON load/search/filter, CRUD for custom packs |
| `src/core/WorkshopDownloader.cpp/.h` | HTTP API client — bakkesplugins.com workshop search + download |
| `src/core/LoadoutManager.cpp/.h` | Car preset cache + game-thread switching |
| `src/core/PackUsageTracker.cpp/.h` | Load count + timestamp tracking → favorites |
| `src/core/TextureDownloader.cpp/.h` | One-time download of 14 .upk workshop textures |
| `src/core/DefaultPacks.h` | `FLICKS_PICKS` curated default training pack list |
| `src/ui/SettingsUI.cpp/.h` | F2 tab: General, Map Selection, Loadout, Hotkeys |
| `src/ui/TrainingPackUI.cpp/.h` | Floating two-panel pack browser PluginWindow |
| `src/ui/LoadoutUI.cpp/.h` | Car preset selection panel |
| `src/ui/StatusMessageUI.cpp/.h` | Reusable toast notifications |
| `src/ui/ConstantsUI.h` | All UI sizes, colors, layout constants |
| `src/ui/HelpersUI.cpp/.h` | ImGui helper widgets (InputIntWithRange, ComboWithTooltip) |
| `src/ui/SuiteSpotIcons.h` | Font Awesome icon definitions (UTF-8 encoded glyph macros) |
| `src/utils/logging.h` | `LOG(...)` macro — spdlog wrapper |

---

## Naming Conventions

- Classes: `CamelCase`
- Functions: `CamelCase`
- Variables: `camelCase`
- Constants: defined in `ConstantsUI.h`, scoped under `UI::`, `UI::SettingsUI::`, etc.
- CVars: all `suitespot_` prefix (one legacy `ss_training_maps`)
- The Hub member for training packs is `trainingPackMgr` (NOT `trainingPackManager`)

---

## Things That Will Crash The Game

1. Calling `LoadFont` during rendering or directly in `SetImGuiContext` (must use `Execute()`)
2. Calling `LoadFont` when the font is already in the atlas (use `GetFont` first)
3. Touching `gameWrapper` or game objects from a background thread (always use `Execute()`)
4. Using `SetTimeout` with 0 delay during game state transitions (minimum 0.1s enforced in `safeExecute`)
5. Hot-reload while the plugin is modifying the font atlas or textures

---

## Post-Build Steps (What Happens After Every Compile)

1. Creates required BakkesMod data directories
2. Copies `assets/icons/` → `data/SuiteSpot/Resources/Icons/`
3. Copies `assets/images/` → `data/SuiteSpot/Resources/` (NoPreview.jpg)
4. Copies `assets/images/NoPreview.jpg` → `data/SuiteSpot/Workshop/NoPreview.jpg` (second copy — this is the one the code reads)
5. Copies `assets/data/training_packs.json` → `data/SuiteSpot/TrainingSuite/`
6. Copies `assets/fonts/` → `data/fonts/`
7. Copies `plugins/SuiteSpot.dll` → BakkesMod plugins folder
8. Patches DLL with `bakkesmod-patch.exe`
9. If Rocket League is running: hot-reloads the plugin
