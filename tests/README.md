# SuiteSpot Tests

Unit tests for components decoupled from the BakkesMod game runtime.
Uses [Catch2](https://github.com/catchorg/Catch2) (header-only, v2).

## Testable components (no live game required)
- `TrainingPackManager` — JSON loading, search, filtering, pagination
- `SettingsSync` — CVar defaults, validation, range clamping
- `PackUsageTracker` — Usage stats, favorites, recency scoring
- `MapManager` — Path resolution, map discovery logic

## Build & run (Linux/WSL2)
```bash
cd tests
make
./suitespot_tests
```

## Add a test
Create a new `.cpp` file in `tests/` and `#include "catch.hpp"`.
The Makefile picks up all `test_*.cpp` files automatically.
