# Changelog

All notable changes to SuiteSpot are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

### Added
- `vcpkg.json` manifest mode — sqlite3, spdlog, fmt, nlohmann-json, openssl, ixwebsocket ready to integrate
- `.clangd` config — full symbol resolution on WSL2 against BakkesMod SDK and vcpkg headers
- `.clang-format` — Allman/attach hybrid style matching existing codebase
- `.clang-tidy` — correctness checks for threading, modern C++20, and BakkesMod plugin patterns
- `tests/` scaffold — Catch2 unit tests for `TrainingPackManager` and `PackUsageTracker`
- `.githooks/pre-commit` — auto-formats staged C++ files with clang-format
- `Doxyfile` — on-demand API documentation generation
- `docs/env-setup-prompt.md` — AI agent environment setup prompt
- `.gitattributes` — enforces CRLF for Windows build files, LF for config/docs
- `$(VCPKG_ROOT)` replaces hardcoded `C:\Users\bmile\vcpkg` in `SuiteSpot.vcxproj`

---

## [3.0.0] — 2025-02

### Fixed
- Video URL button overlap with row selectable area
- Workshop download reliability and search filtering
- Workshop auto-fetch on load, header clock display
- Critical fixes for game-speed-fix feature integration
- Missing YouTube play button icon
- Event hook cleanup on hot-reload (`onUnload`)

### Changed
- Production hardening: process safety, thread lifecycle
- AutoLoadFeature error handling and logging improvements
- TrainingPackUI and AutoLoadFeature code clarity and consistency
- Game-speed-fix branch cleanup and refactor
- Moved RAG documentation system to separate repository

### Added
- Workshop download auto-fetch
- Header clock display

---

## [2.1.0]

### Added
- Workshop map downloader (RLMAPS API integration)
- Texture downloader
- Loadout manager and UI
- Pack usage tracker with favorites and recency scoring
- Status message UI (toast notifications)

---

## [2.0.0]

### Added
- Training pack browser with 2300+ packs
- Search and filter by name, difficulty, tags
- Quick picks / favorites system
- Settings UI (F2 menu, Map Select / Loadout / Workshop tabs)
- Post-match auto-load with configurable delay per map type
- CVar-backed settings with auto-persistence

---

*Older history available via `git log --oneline`.*
