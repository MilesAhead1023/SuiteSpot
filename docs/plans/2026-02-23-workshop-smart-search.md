# Workshop Smart Search Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add real-time relevance-ranked search to the workshop browser so typing "f" instantly shows all maps containing "f", ranked with best matches at top — all on one page, no pagination.

**Architecture:** Add a `displayResultList` (ranked view) alongside the existing `cachedResultList` (unmodified API results). All existing API/download/image/mutex code is untouched. The ranking is a pure client-side sort applied after results arrive. A local filter input re-ranks without hitting the API again.

**Tech Stack:** C++20, ImGui 1.75, existing `WorkshopDownloader` / `SettingsUI` classes

---

## ⚠️ REGRESSION RISK — READ BEFORE TOUCHING ANYTHING

The workshop browser took a month to stabilize. These things must **not change**:

- `WorkshopDownloader.cpp/.h` — Do NOT touch API calls, mutex, thread, image download, `listVersion`, `searchGeneration`
- `RenderReleases()` — Do NOT touch download popup/thread logic
- `RenderAcceptDownload()`, `RenderYesNoPopup()`, `RenderInfoPopup()` — Do NOT touch
- `cachedResultList` filtering logic (lines 940–956 in SettingsUI.cpp) — preserve exactly
- `workshopImageCache` keyed by map ID — preserve exactly
- `selectedBrowserIndex` clamping (lines 962–963) — preserve, just change which list it indexes

**Safe zone:** We only add new members and a new `displayResultList`. Rendering switches from `cachedResultList` to `displayResultList`. The existing `cachedResultList` remains the unmodified source of truth.

---

## Task 1: Add scoring function to SettingsUI.cpp

**Files:**
- Modify: `SettingsUI.cpp` (add before `RenderWorkshopBrowserTab`, around line 832)

**What:** Add a pure static free function `ScoreResult()`. No class state, no side effects. Safe to add anywhere above the functions that use it.

**Step 1: Add the function**

Insert this block immediately above `void SettingsUI::RenderWorkshopBrowserTab()` (line 832):

```cpp
// ── Workshop Search Scoring ────────────────────────────────────────────────
// Returns relevance score >= 1 if query found in result, -1 if no match.
// Higher = better match. Uses search-engine best practices:
//   1000 = exact name match
//    900 = name starts with query (prefix)
//    800 = any word in name starts with query (word boundary)
//    700-N = query found at position N in name (earlier = better)
//    +50 per extra occurrence (frequency bonus)
//    +100 if author also matches
static int ScoreResult(const std::string& queryLower, const RLMAPS_MapResult& result)
{
    if (queryLower.empty()) return 1; // No filter: everything passes

    std::string nameLower = result.Name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    size_t firstPos = nameLower.find(queryLower);
    if (firstPos == std::string::npos) return -1; // No match → exclude

    int score = 0;

    // Tier 1: exact match
    if (nameLower == queryLower) {
        score = 1000;
    }
    // Tier 2: prefix match (name starts with query)
    else if (nameLower.compare(0, queryLower.size(), queryLower) == 0) {
        score = 900;
    }
    // Tier 3: word boundary match
    else {
        bool wordBoundary = false;
        // A word boundary = position 0 or preceded by space
        size_t searchPos = 0;
        while ((searchPos = nameLower.find(queryLower, searchPos)) != std::string::npos) {
            if (searchPos == 0 || nameLower[searchPos - 1] == ' ') {
                wordBoundary = true;
                break;
            }
            searchPos++;
        }
        if (wordBoundary) {
            score = 800;
        } else {
            // Tier 4: position-based (max 700, minus 10 per character of distance)
            score = std::max(1, 700 - (int)firstPos * 10);
        }
    }

    // Frequency bonus: extra +50 per additional occurrence after the first
    size_t countPos = 0;
    int occurrences = 0;
    while ((countPos = nameLower.find(queryLower, countPos)) != std::string::npos) {
        occurrences++;
        countPos += queryLower.size();
    }
    if (occurrences > 1) score += (occurrences - 1) * 50;

    // Author bonus: +100 if query appears in author name too
    std::string authorLower = result.Author;
    std::transform(authorLower.begin(), authorLower.end(), authorLower.begin(), ::tolower);
    if (!authorLower.empty() && authorLower.find(queryLower) != std::string::npos)
        score += 100;

    return std::max(score, 1);
}
```

**Step 2: Build and verify no compile errors**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Expected: `SuiteSpot.vcxproj -> ...SuiteSpot.dll` with no errors.

**Step 3: Commit**

```bash
git add SettingsUI.cpp
git commit -m "feat(search): add ScoreResult relevance scoring function"
```

---

## Task 2: Add displayResultList and localFilterBuf to SettingsUI.h

**Files:**
- Modify: `SettingsUI.h`

**What:** Add two new private members. `displayResultList` is the ranked view rendered in the UI. `localFilterBuf` holds the user's real-time filter text. No existing members change.

**Step 1: Add members**

In `SettingsUI.h`, find the `// Workshop browser (RLMAPS) state` block (lines 81–84):

```cpp
    // Workshop browser (RLMAPS) state
    int selectedBrowserIndex = -1;  // Currently selected in browser list
    std::vector<RLMAPS_MapResult> cachedResultList;
    int lastListVersion = -1; // Track version to know when to refresh cache
```

Change to:

```cpp
    // Workshop browser (RLMAPS) state
    int selectedBrowserIndex = -1;       // Indexes into displayResultList
    std::vector<RLMAPS_MapResult> cachedResultList;  // Unmodified API results (source of truth)
    std::vector<RLMAPS_MapResult> displayResultList; // Ranked/filtered view — what the UI renders
    int lastListVersion = -1;            // Track version to know when to refresh cache
    char localFilterBuf[256] = {0};      // Real-time local filter (re-ranks without API call)
    std::string lastLocalFilter;         // Detect when filter text changes
```

Also add the private helper declaration. Find the existing private methods and add:

```cpp
    void RebuildDisplayList(); // Re-scores and sorts displayResultList from cachedResultList
```

**Step 2: Build**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Expected: build succeeds (linker error on `RebuildDisplayList` is OK — we implement it next).

**Step 3: Commit**

```bash
git add SettingsUI.h
git commit -m "feat(search): add displayResultList and localFilterBuf state"
```

---

## Task 3: Implement RebuildDisplayList()

**Files:**
- Modify: `SettingsUI.cpp` (add after `ScoreResult`, before `RenderWorkshopBrowserTab`)

**What:** `RebuildDisplayList()` reads `cachedResultList` + `localFilterBuf`, scores every entry, filters out negatives, sorts descending by score, writes result into `displayResultList`. Resets selection.

**Step 1: Add implementation**

Insert immediately after the `ScoreResult` function added in Task 1:

```cpp
void SettingsUI::RebuildDisplayList()
{
    std::string query = localFilterBuf;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    // Score and filter
    std::vector<std::pair<int, size_t>> scored;
    scored.reserve(cachedResultList.size());
    for (size_t i = 0; i < cachedResultList.size(); ++i) {
        int s = ScoreResult(query, cachedResultList[i]);
        if (s > 0) scored.push_back({s, i});
    }

    // Sort descending by score
    std::stable_sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // Build display list
    displayResultList.clear();
    displayResultList.reserve(scored.size());
    for (auto& [score, idx] : scored)
        displayResultList.push_back(cachedResultList[idx]);

    // Reset selection — position in list may have shifted
    selectedBrowserIndex = -1;
}
```

**Step 2: Build**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Expected: clean build, no linker errors.

**Step 3: Commit**

```bash
git add SettingsUI.cpp
git commit -m "feat(search): implement RebuildDisplayList scoring and sorting"
```

---

## Task 4: Wire RebuildDisplayList into RLMAPS_RenderSearchWorkshopResults

**Files:**
- Modify: `SettingsUI.cpp` — `RLMAPS_RenderSearchWorkshopResults()` (lines 931–957)

**What:** Call `RebuildDisplayList()` after `cachedResultList` is built. Also call it when `localFilterBuf` changes. Switch all rendering from `cachedResultList` to `displayResultList`. This is the only place where `cachedResultList` and `displayResultList` diverge.

**Step 1: Modify the version-check block**

Find this block (lines 934–957):

```cpp
    // Check if list has changed
    int currentVersion = plugin_->workshopDownloader->listVersion.load();
    if (currentVersion != lastListVersion) {
        std::lock_guard<std::mutex> lock(plugin_->workshopDownloader->resultsMutex);
        auto& fullList = plugin_->workshopDownloader->RLMAPS_MapResultList;
        cachedResultList.clear();
        for (auto& result : fullList) {
            // Filter out maps that already exist in the download directory
            std::string safeName = plugin_->workshopDownloader->SanitizeMapName(result.Name);
            std::string mapDir = std::string(mapspath);
            if (!mapDir.empty() && (mapDir.back() != '/' && mapDir.back() != '\\'))
                mapDir += "/";
            mapDir += safeName;
            if (!fs::exists(mapDir)) {
                cachedResultList.push_back(result);
            }
        }
        LOG("UI Synced list. New version: {}, items: {} (filtered from {})",
            currentVersion, cachedResultList.size(), fullList.size());
        lastListVersion = currentVersion;
        // Reset selection if out of range
        if (selectedBrowserIndex >= (int)cachedResultList.size())
            selectedBrowserIndex = -1;
    }
```

Replace with:

```cpp
    // Check if API list has changed
    int currentVersion = plugin_->workshopDownloader->listVersion.load();
    if (currentVersion != lastListVersion) {
        std::lock_guard<std::mutex> lock(plugin_->workshopDownloader->resultsMutex);
        auto& fullList = plugin_->workshopDownloader->RLMAPS_MapResultList;
        cachedResultList.clear();
        for (auto& result : fullList) {
            // Filter out maps that already exist in the download directory
            std::string safeName = plugin_->workshopDownloader->SanitizeMapName(result.Name);
            std::string mapDir = std::string(mapspath);
            if (!mapDir.empty() && (mapDir.back() != '/' && mapDir.back() != '\\'))
                mapDir += "/";
            mapDir += safeName;
            if (!fs::exists(mapDir)) {
                cachedResultList.push_back(result);
            }
        }
        LOG("UI Synced list. New version: {}, items: {} (filtered from {})",
            currentVersion, cachedResultList.size(), fullList.size());
        lastListVersion = currentVersion;
        RebuildDisplayList(); // Score and sort the freshly-built cache
    }

    // Re-rank if user changed the local filter text
    std::string currentFilter = localFilterBuf;
    if (currentFilter != lastLocalFilter) {
        lastLocalFilter = currentFilter;
        RebuildDisplayList();
    }
```

**Step 2: Switch rendering from cachedResultList to displayResultList**

Find every use of `cachedResultList` in `RLMAPS_RenderSearchWorkshopResults()` AFTER the version-check block and replace with `displayResultList`. There are these occurrences:

Line 959: `if (cachedResultList.empty()) return;`
→ `if (displayResultList.empty()) return;`

Line 962–963:
```cpp
    if (selectedBrowserIndex >= (int)cachedResultList.size())
        selectedBrowserIndex = (int)cachedResultList.size() - 1;
```
→
```cpp
    if (selectedBrowserIndex >= (int)displayResultList.size())
        selectedBrowserIndex = (int)displayResultList.size() - 1;
```

Line 975: `ImGui::TextDisabled("%d maps", (int)cachedResultList.size());`
→ `ImGui::TextDisabled("%d maps", (int)displayResultList.size());`

Line 978: `for (int i = 0; i < (int)cachedResultList.size(); i++) {`
→ `for (int i = 0; i < (int)displayResultList.size(); i++) {`

Line 979: `auto& mapResult = cachedResultList[i];`
→ `auto& mapResult = displayResultList[i];`

Line 1007: `if (selectedBrowserIndex >= 0 && selectedBrowserIndex < (int)cachedResultList.size()) {`
→ `if (selectedBrowserIndex >= 0 && selectedBrowserIndex < (int)displayResultList.size()) {`

Line 1008: `auto& mapResult = cachedResultList[selectedBrowserIndex];`
→ `auto& mapResult = displayResultList[selectedBrowserIndex];`

**Step 3: Build**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Expected: clean build.

**Step 4: Commit**

```bash
git add SettingsUI.cpp
git commit -m "feat(search): wire displayResultList into render — all results now ranked"
```

---

## Task 5: Add local filter input to RenderWorkshopBrowserTab

**Files:**
- Modify: `SettingsUI.cpp` — `RenderWorkshopBrowserTab()` (lines 874–898)

**What:** Add a second search input below the API Search button. Typing here re-ranks results already in memory without an API call. Replace the existing count display (line 892–893) with the filter input.

**Step 1: Replace the search bar section**

Find this block (lines 874–898):

```cpp
    // Search bar
    ImGui::Text("Search Maps:");
    ImGui::SetNextItemWidth(400.0f);
    bool enterPressed = ImGui::InputText("##WorkshopSearch", workshopSearchBuf, IM_ARRAYSIZE(workshopSearchBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    if ((ImGui::Button("Search") || enterPressed) && strlen(workshopSearchBuf) > 0) {
        plugin_->workshopDownloader->GetResults(workshopSearchBuf, 1);
    }

    ImGui::SameLine();
    if (plugin_->workshopDownloader->RLMAPS_Searching) {
        if (ImGui::Button("Stop")) {
            plugin_->workshopDownloader->StopSearch();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Searching...");
    } else if (!cachedResultList.empty()) {
        ImGui::Text("%d maps found", (int)cachedResultList.size());
    }
```

Replace with:

```cpp
    // API Search — fetches from RLMAPS
    ImGui::Text("Search Maps:");
    ImGui::SetNextItemWidth(400.0f);
    bool enterPressed = ImGui::InputText("##WorkshopSearch", workshopSearchBuf, IM_ARRAYSIZE(workshopSearchBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    if ((ImGui::Button("Search") || enterPressed) && strlen(workshopSearchBuf) > 0) {
        // Reset local filter when doing a new API search
        memset(localFilterBuf, 0, sizeof(localFilterBuf));
        lastLocalFilter.clear();
        plugin_->workshopDownloader->GetResults(workshopSearchBuf, 1);
    }

    ImGui::SameLine();
    if (plugin_->workshopDownloader->RLMAPS_Searching) {
        if (ImGui::Button("Stop")) {
            plugin_->workshopDownloader->StopSearch();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Searching...");
    } else if (!cachedResultList.empty()) {
        ImGui::Text("%d / %d maps", (int)displayResultList.size(), (int)cachedResultList.size());
    }

    // Local filter — re-ranks already-loaded results in real time, no API call
    if (!cachedResultList.empty()) {
        ImGui::Spacing();
        ImGui::Text("Filter & Rank:");
        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputText("##LocalFilter", localFilterBuf, IM_ARRAYSIZE(localFilterBuf));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Type to filter and rank results by relevance. Closer matches appear first.");
        if (strlen(localFilterBuf) > 0) {
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                memset(localFilterBuf, 0, sizeof(localFilterBuf));
                lastLocalFilter.clear();
            }
        }
    }
```

**Step 2: Build**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Expected: clean build. Hot-reload triggers. Plugin loads.

**Step 3: Commit**

```bash
git add SettingsUI.cpp
git commit -m "feat(search): add local filter input with real-time ranking"
```

---

## Task 6: Manual smoke test

**In-game checklist (do NOT skip):**

1. Open F2 → SuiteSpot → Workshop Browser tab
2. Verify: existing search box still works (type "test", press Search) → results load
3. Verify: results display in left panel, details in right panel (unchanged layout)
4. Verify: clicking a map selects it, shows preview + download button (unchanged)
5. Type in **Filter & Rank** box → results re-order in real time
6. Type "f" → maps with "f" in name appear, best matches (prefix/exact) first
7. Clear filter → all API results show again
8. Download a map → download flow works (popup, progress bar, extraction)
9. Verify: no crash on hot-reload

**If anything in steps 1-4 or 8-9 breaks → STOP, revert last commit, investigate before continuing.**

---

## Task 7: Push and update PR

```bash
git push
```

PR #23 already open on `qol/touchups`. The new commits will appear automatically.

Update the PR description to include the smart search feature.
