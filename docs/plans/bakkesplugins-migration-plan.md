# BakkesPlugins API Migration Plan

## Problem
The workshop map browser backend uses RLMAPS (`celab.jetfox.ovh`) which requires **N+1 HTTP calls** (1 search + 1 `/releases` call per map) to populate the list, has no social metrics, no CDN, and returns noisy uncurated data.

## Solution
Replace with **BakkesPlugins API** (`bakkesplugins.com`) — one list call returns 285 curated maps with CDN thumbnail URLs, download counts, ratings, and tags baked in. Files (`edgeUrl`) are lazy-fetched per-map only when user clicks Download.

## New API Call Pattern
- **Old**: `search → N × /releases` (N+1 calls, 50ms stagger, parallel threads)  
- **New**: `GET /api/rocket-league-maps?search=X&pageSize=285&page=1` (1 call), lazy `GET /api/rocket-league-maps/{id}` only when user clicks Download

---

## Files Changed

1. `WorkshopDownloader.h`
2. `WorkshopDownloader.cpp`
3. `SettingsUI.h`
4. `SettingsUI.cpp`

---

## Todos

### 1. Replace data structs in WorkshopDownloader.h
- Remove `RLMAPS_Release` struct
- Add `BP_MapFile` struct: `{ int id; string fileName; string edgeUrl; int64_t fileSizeBytes; string fileHash; string versionString; int downloadCount; }`
- Replace `RLMAPS_MapResult` with `BP_MapResult`: `{ int id; string Name; string Description; string Author; string bannerUrl; int downloadCount; int viewCount; float averageRating; int ratingCount; vector<string> tags; vector<BP_MapFile> files; bool filesLoaded; fs::path ImagePath; string ImageExtension; bool isImageLoaded; bool IsDownloadingPreview; }`
- Update `RLMAPS_MapResultList` → `BP_MapResultList` (type change)
- Replace `rlmaps_url` with `bp_base_url = "https://bakkesplugins.com/api/rocket-league-maps"`
- Update method signatures: remove `FetchReleaseDetails`, remove `FetchImageOnly`, add `FetchMapFiles(int index, int generation)`, update `RLMAPS_DownloadWorkshop` → `DownloadWorkshop` signature to use `BP_MapFile`

### 2. Rewrite GetResults() in WorkshopDownloader.cpp
- Build URL: `bp_base_url + "?search=" + keyword + "&pageSize=285&page=1&sortBy=downloads"`
- Parse paginated envelope: `json["items"]` array (not a raw array)
- Map each item: `id` (int→string for cache compat or keep int), `name`, `description`, `member.displayName` → Author, `bannerUrl`, `downloadCount`, `viewCount`, `averageRating`, `ratingCount`, `tags[].key`
- Populate `BP_MapResultList` in one pass — no secondary threads needed
- After list is built, spawn image fetch threads for each map using `bannerUrl` directly (replaces the FetchReleaseDetails → DownloadPreviewImage chain)
- Image cache file path: `BakkesmodPath + "SuiteSpot\\Workshop\\img\\" + std::to_string(id) + ext`

### 3. Remove FetchReleaseDetails() and FetchImageOnly(), add FetchMapFiles()
- Delete `FetchReleaseDetails()` body entirely
- Delete `FetchImageOnly()` body entirely
- Add `FetchMapFiles(int index, int generation)`: calls `GET /api/rocket-league-maps/{id}`, parses `files[]` into `BP_MapFile`, sets `filesLoaded = true`, increments `listVersion`
- This is called lazily when user selects a map that hasn't had its files loaded yet

### 4. Rewrite DownloadWorkshop() in WorkshopDownloader.cpp
- Change signature: `DownloadWorkshop(string folderpath, BP_MapResult mapResult, BP_MapFile file)`
- Use `file.edgeUrl` as download URL (replaces `release.downloadLink`)
- Use `file.fileSizeBytes` for `RLMAPS_WorkshopDownload_FileSize` (exact, not `RLMAPS_WorkshopDownload_Progress` placeholder)
- Use `file.fileName` as zip name (replaces `release.zipName`)
- Rest of download/extract/rename logic unchanged

### 5. Update SettingsUI.h
- Change `cachedResultList` / `displayResultList` type from `vector<RLMAPS_MapResult>` → `vector<BP_MapResult>`
- Change `workshopImageCache` key from `string` (was `mapResult.ID`) to `int` (or keep as `string` using `std::to_string(id)`)
- Change `RenderReleases(RLMAPS_MapResult, const char*)` → `RenderFiles(BP_MapResult, const char*)`

### 6. Update ScoreResult() and RebuildDisplayList() in SettingsUI.cpp
- `result.Name` → unchanged
- `result.Author` → unchanged  
- Add: score +50 if query matches any `result.tags` entry
- Add: score +25 if query appears in `result.Description`
- Add sort controls: `sortBy` enum (Downloads, Views, Rating, Newest) — applied in `RebuildDisplayList()` as secondary sort after relevance score

### 7. Update RLMAPS_RenderSearchWorkshopResults() in SettingsUI.cpp
- Image cache lookup: change `mapResult.ID` → `std::to_string(mapResult.id)`  
- Left panel: `hasReleases` → `hasFiles = mapResult.filesLoaded` check for greying out
- Left panel: show download count next to name: `"MapName (38k ↓)"` or similar
- Right panel: show rating stars, download count, view count, tags above description
- Right panel: trigger `FetchMapFiles()` when a map is selected and `!filesLoaded`
- Replace `"Loading releases..."` text → `"Loading files..."` when `!filesLoaded`

### 8. Update RenderReleases() → RenderFiles() in SettingsUI.cpp
- Rename to `RenderFiles(BP_MapResult mapResult, const char* mapspath)`
- Iterate `mapResult.files` (vector of `BP_MapFile`) instead of `mapResult.releases`
- Button label: `file.versionString` (was `release.tag_name`)
- Show file size: `file.fileSizeBytes / 1024 / 1024` MB
- Thread spawns `DownloadWorkshop(path, mapResult, file)` (was `RLMAPS_DownloadWorkshop`)

---

## New UI Additions (opportunistic, low-risk)

### Sort-By Dropdown
Add `ImGui::Combo("Sort##ws", ...)` with options: Downloads ↓, Views ↓, Rating ↓, Newest. Applied as tiebreaker in `RebuildDisplayList()` when local filter is empty.

### Tag Filter Chips
Since server-side tag filtering is broken, implement client-side: show tag buttons (aerial, dribble, challenge, rumble, etc.) above results. Clicking a tag adds it to active filter set; `RebuildDisplayList()` excludes maps without that tag.

---

## Key Constraints / Notes
- `files[]` is NOT in the list response — only in `GET /api/rocket-league-maps/{id}`. Lazy-fetch on selection.
- Tag filtering must be client-side (server silently ignores all tag params).
- `sortBy` query param works server-side for initial load. Use `sortBy=downloads` as default to show popular maps first.
- `pageSize=285` returns all maps in one call (verified: totalCount=285).
- Image cache path changes from `mapId_string + ext` to `std::to_string(id) + ext`. Old cached images will be re-downloaded once (acceptable).
- Keep `RLMAPS_IsDownloadingWorkshop`, `RLMAPS_WorkshopDownload_Progress`, `RLMAPS_WorkshopDownload_FileSize` atomic names — they're public and referenced from SettingsUI.cpp. Rename optional.
- `GetNumPages()` can be removed or simplified (BakkesPlugins returns `totalPages` in envelope).
