# Changelog

All notable changes to SuiteSpot will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.0.0] — Initial Release

### Added

#### Core Automation
- Post-match auto-load: automatically loads Freeplay, Training Pack, or Workshop Map when any match ends
- Per-mode configurable delays (Freeplay, Training, Workshop) — prevents crash-on-state-transition
- Auto-Queue: optionally queues for a new match after training with configurable delay

#### Training Pack Browser
- Searchable, filterable browser with 2300+ community training packs
- Full-text search across pack name, creator, and share code simultaneously
- Filters: difficulty, tags, shot count range slider, video-only toggle
- Sortable columns: name, creator, difficulty, shot count (ascending/descending)
- Virtual scrolling via `ImGuiListClipper` — smooth performance at full database size
- YouTube thumbnail lazy-loading for packs with preview videos
- Custom pack management: add, edit, and delete user-defined packs with full metadata

#### Quick Picks
- Flicks Picks: curated high-quality community pack list (`src/core/DefaultPacks.h`)
- Your Favorites: usage-ranked packs from `PackUsageTracker`, sorted by load count × recency
- Configurable count of quick picks to cycle through

#### Workshop Maps
- RLMAPS API integration — search and download workshop maps from within BakkasMod
- Two-panel local workshop browser with map preview images
- Automatic download pipeline: ZIP download → PowerShell extract → `.udk`→`.upk` rename → JSON metadata creation
- Preview image download and caching per map ID
- Selection persistence via string map ID (not fragile index)

#### Hotkeys
- Five configurable dual-key combo actions:
  - Cycle map mode forward / backward
  - Cycle map forward / backward
  - Load now
- Hotkeys captured via `HandleKeyPress` hook with `heldKeys` tracking
- In-settings capture UI with visual key assignment

#### Quality of Life
- Loadout Manager: car preset switching during training sessions via game-thread-safe `Execute()`
- Pack Healer: hooks `GameEvent_TrainingEditor_TA.OnInit`, reads actual shot count and corrects pack database
- `ss_heal_current_pack` console notifier for manual pack healing
- Training Game Speed Fix: re-applies `sv_soccar_gamespeed` after loading a training pack
- Texture Auto-Installer: background download of 14 required workshop editor textures on first run
- Usage Tracker: persists load count and timestamp per pack code to `pack_usage_stats.json`

#### UI System
- `UI::StatusMessage` reusable toast notification class with three display modes:
  - `Timer` — instant hide after duration
  - `TimerWithFade` — smooth alpha fade-out
  - `ManualDismiss` — user must click Dismiss
- Four message types: Success (green), Error (red), Warning (yellow), Info (blue)
- `UI::Helpers` reusable widget library: `InputIntWithRange`, `ComboWithTooltip`, `ButtonWithTooltip`, `CheckboxWithCVar`, `InputTextWithTooltip`, `ExecuteCommandSafely`
- Centralized `UI::ConstantsUI` styling constants for consistent look and feel
- Three custom fonts: Ubuntu-Regular 48px (clock overlay), Roboto-Medium 14px (UI), FA5 Solid icons merged into Roboto

#### Infrastructure
- Hub-and-Spoke architecture with `src/SuiteSpot.cpp` as the central coordinator
- Source organized into `src/core/`, `src/ui/`, `src/utils/` subdirectories
- All settings use `suitespot_*` CVar prefix, persisted automatically to `config.cfg`
- CI/CD: GitHub Actions build on every push, artifact upload on tagged releases
- `CompiledScripts/SuitePackGrabber.ps1` — PowerShell scraper to update training pack database (2300+ packs, ~2.6MB)
- `CompiledScripts/update_version.ps1` — pre-build automatic version bumper
- Catch2 unit tests for non-SDK components (`tests/`)
- Doxygen configuration for API documentation generation
