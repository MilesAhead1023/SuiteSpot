# SuiteSpot External API Map

All HTTP requests are made via BakkesMod's `HttpWrapper::SendCurlRequest()`.  
Auth required: **None** (all endpoints are public read-only)

---

## 1. RLMAPS API (Workshop Map Search & Download)

Base URL: `https://celab.jetfox.ovh/api/v4`  
Infrastructure: **GitLab** self-hosted instance  

---

### Search Maps
```
GET /projects/?search={keyword}&per_page=1000&page={page}
```

**Query Parameters:**
| Param | Type | Description |
|---|---|---|
| `search` | string | Keyword to search map names |
| `per_page` | int | Results per request — plugin hardcodes `1000` |
| `page` | int | Page number (1-based) |

**Used by:** `WorkshopDownloader::GetResults()`, `WorkshopDownloader::GetNumPages()`

**Response:** JSON array of project objects.

**Project Object Schema** (fields consumed by plugin):
```jsonc
[
  {
    "id": 42,                          // int|string — unique GitLab project ID; plugin stores as string
    "name": "Lethamyr Ice Rings",      // string — map display name
    "path": "lethamyr-ice-rings",      // string — URL-safe slug; used to construct package image URLs
    "description": "<p>HTML desc</p>", // string — HTML-encoded; plugin strips tags via CleanHTML()
    "namespace": {
      "path": "lethamyr"               // string — author/group name
    }
    // ... many additional GitLab fields (ignored by plugin)
  }
]
```

**Error behavior:** Non-200 response sets `SearchErrorBool = true` with message `"Search failed: HTTP {code}. RLMAPS API may be down."`.

---

### Get Map Releases
```
GET /projects/{id}/releases
```

**Path Parameters:**
| Param | Type | Description |
|---|---|---|
| `id` | int | GitLab project ID from search results |

**Used by:** `WorkshopDownloader::FetchReleaseDetails()` — called in parallel for every result after a search (one thread per map, 50ms stagger between spawns).

**Response:** JSON array of release objects, one per published release.

**Release Object Schema** (fields consumed by plugin):
```jsonc
[
  {
    "name": "v1.0",                    // string → RLMAPS_Release::name
    "tag_name": "v1.0",                // string → RLMAPS_Release::tag_name
    "description": "Release notes",   // string → RLMAPS_Release::description
    "assets": {
      "links": [
        {
          "name": "IceRings.zip",      // string — filename; used to identify download vs image
          "url": "https://...",        // string → RLMAPS_Release::downloadLink (if .zip)
          "link_type": "other"         // string — "image" triggers pictureLink assignment; "other" for .zip files
        },
        {
          "name": "preview.jpg",       // string — image name (.jpg/.jpeg/.png/.jfif/.webp → pictureLink)
          "url": "https://...",        // string → RLMAPS_Release::pictureLink
          "link_type": "image"         // string
        }
      ]
    }
    // ... additional GitLab release fields (ignored)
  }
]
```

**Link classification logic:**
- **Image link:** `link_type == "image"` OR name ends with `.jpg`, `.jpeg`, `.png`, `.jfif`, `.webp` → `RLMAPS_Release::pictureLink` + `RLMAPS_MapResult::PreviewUrl`
- **Download link:** name ends with `.zip` → `RLMAPS_Release::downloadLink` + `RLMAPS_Release::zipName`
- First match of each type wins (subsequent links of same type are ignored)

---

### Get Map Packages
```
GET /projects/{id}/packages
```

**Path Parameters:**
| Param | Type | Description |
|---|---|---|
| `id` | int | GitLab project ID |

**Used by:** `WorkshopDownloader::FetchImageOnly()` — lightweight fallback that skips releases and constructs image URL from package metadata only.

**Response:** JSON array of package objects; plugin uses the **last** entry (assumed to be latest).

**Package Object Schema** (fields consumed by plugin):
```jsonc
[
  {
    "name": "lethamyr-ice-rings",      // string — package name; used in image URL construction
    "version": "v1.0"                  // string — package version; used in image URL construction
    // ... additional GitLab package fields (ignored)
  }
]
```

**Constructed image URL pattern:**
```
https://celab.jetfox.ovh/api/v4/projects/{id}/packages/generic/{packageName}/{version}/{packageName}.jpg
```
Example:
```
https://celab.jetfox.ovh/api/v4/projects/42/packages/generic/lethamyr-ice-rings/v1.0/lethamyr-ice-rings.jpg
```
> ⚠️ Extension is hardcoded to `.jpg` in `FetchImageOnly()` — does not attempt `.png`/`.webp` variants.

---

### Download Preview Image (any URL)
```
GET {previewUrl}    ← arbitrary URL extracted from releases or packages
```

**Used by:** `WorkshopDownloader::DownloadPreviewImage()` — fires after `FetchReleaseDetails` or `FetchImageOnly` resolves a preview URL.

**Behavior:**
- Downloads raw image bytes and writes to local cache: `%AppData%\bakkesmod\bakkesmod\data\SuiteSpot\Workshop\img\{mapId}{ext}`
- Extension detected from URL via `GetImageExtension()`: `.png`, `.jpg`, `.jpeg`, `.webp`, `.gif`, or `.jfif` (default)
- Result assigned to `RLMAPS_MapResult::ImagePath` + `isImageLoaded = true`
- Skipped if file already exists in cache

---

### Download Workshop Map (zip)
```
GET {release.downloadLink}    ← direct CDN/storage URL from the releases assets.links array
```

**Used by:** `WorkshopDownloader::RLMAPS_DownloadWorkshop()`

**Behavior:**
- Requires user confirmation via `UserIsChoosingYESorNO` spin-wait (UI interaction)
- Streams binary zip to: `{workshopFolder}/{sanitizedMapName}/{release.zipName}`
- Tracks download progress via `req.progress_function` → `RLMAPS_Download_Progress` / `RLMAPS_WorkshopDownload_FileSize`
- After write: extracts zip with `Utils::ExpandArchive()`, then renames `.udk` file via `RenameFileToUPK()`

---

## Endpoint Reference (RLMAPS)

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| GET | `/api/v4/projects/?search={kw}&per_page=1000&page={n}` | None | Search workshop maps |
| GET | `/api/v4/projects/{id}/releases` | None | All releases for a map (download links + preview images) |
| GET | `/api/v4/projects/{id}/packages` | None | Package metadata (used to construct image URL) |
| GET | `/api/v4/projects/{id}/packages/generic/{name}/{ver}/{name}.jpg` | None | Preview image (constructed URL, not enumerated) |
| GET | `{release.downloadLink}` | None | Download workshop map zip (URL from releases response) |
| GET | `{previewUrl}` | None | Download preview image (URL from releases or constructed from packages) |

---

## 2. prejump.com (Training Pack Database)

Base URL: `https://prejump.com`  
**Not a REST API** — the plugin scrapes HTML pages. Data is embedded in the page HTML as a `data-page` attribute containing JSON-encoded props.

---

### Scrape Training Pack Page
```
GET /training-packs?page={page}
```

**Query Parameters:**
| Param | Type | Description |
|---|---|---|
| `page` | int | Page number (1-based, ~231 pages) |

**Used by:** `EmbeddedPackGrabber.h` PowerShell script (`Invoke-PrejumpPageScrape` + `Extract-PackDataFromHtml`), which is embedded in the DLL and extracted to a temp file at runtime via `TrainingPackManager`.

**Rate limiting:** 200ms delay between page requests (`Start-Sleep -Milliseconds 200`).

**User-Agent:** `Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36`

**Response:** Full HTML page. The plugin extracts the `data-page="..."` attribute (HTML-entity-encoded JSON) and decodes it with `[System.Web.HttpUtility]::HtmlDecode()`.

**Extracted JSON structure:**
```jsonc
{
  "props": {
    "packs": {
      "data": [ ...Pack ],             // array — training packs on this page (10 per page)
      "meta": {
        "total": 2301,                 // int — total pack count across all pages
        "last_page": 231,              // int — total page count
        "current_page": 1              // int — current page number
      }
    }
  }
}
```

**Pack Object Schema** (fields consumed by plugin):
```jsonc
{
  "name": "Dribbling Course v2",       // string → name
  "code": "A1B2-C3D4-E5F6-G7H8",      // string → code (Rocket League training pack code, used as unique ID)
  "creator": "Lethamyr",               // string → creator
  "creatorSlug": "lethamyr",           // string → creatorSlug (URL-safe)
  "difficulty": "Intermediate",        // string → difficulty
  "shotCount": 10,                     // int → shotCount
  "tags": ["dribble", "aerial"],       // string[] → tags
  "videoUrl": "https://youtube.com/...", // string → videoUrl
  "staffComments": "Great pack!",      // string → staffComments
  "notes": "...",                      // string → notes
  "likes": 142,                        // int → likes (dynamic, updated on re-scrape)
  "plays": 3821,                       // int → plays (dynamic, updated on re-scrape)
  "status": "published"                // string → status
}
```

**Output JSON schema** (written to `training_packs.json`):
```jsonc
{
  "version": "1.1.0",
  "lastUpdated": "2026-02-23T19:40:16Z",  // ISO 8601
  "source": "https://prejump.com/training-packs",
  "totalPacks": 2301,
  "packs": [ ...NormalizedPack ]
}
```

**NormalizedPack** (superset of Pack, adds plugin-managed fields):
```jsonc
{
  // All Pack fields, plus:
  "source": "prejump",               // string: "prejump" | "custom"
  "bagCategories": [],               // string[] — user-assigned categories; preserved across updates
  "isModified": false                // bool — if true, user edits are preserved and only likes/plays are updated
}
```

**Merge behavior:**
- `source == "custom"` packs are preserved entirely (not overwritten by scrape)
- `isModified == true` packs retain all user edits; only `likes` and `plays` are refreshed from the live data
- Deduplication by `code` field (first occurrence wins)

---

## 3. Google Drive (Workshop Textures)

```
GET https://drive.usercontent.google.com/download?id=1jklpjfEu4Yw97cjYaMDWRx8H2XFyji6U&export=download&confirm=t
```

**Used by:** `TextureDownloader::DownloadAndInstallTextures()`

**Parameters (hardcoded):**
| Param | Value | Description |
|---|---|---|
| `id` | `1jklpjfEu4Yw97cjYaMDWRx8H2XFyji6U` | Google Drive file ID for the textures zip |
| `export` | `download` | Triggers direct file download |
| `confirm` | `t` | Bypasses Google's large-file warning |

**Behavior:**
- Downloads zip binary to: `%AppData%\bakkesmod\bakkesmod\data\SuiteSpot\Workshop\Textures.zip`
- Tracks progress via `downloadProgress` (0–100)
- Extracts to Rocket League's `TAGame\CookedPCConsole\` directory
- Only triggered when `TextureDownloader::CheckMissingTextures()` finds files absent from the game directory
- Extraction runs in a dedicated thread to avoid freezing the game

---

## Full Endpoint Reference

| Method | Base | Path / URL | Auth | Plugin Consumer | Description |
|---|---|---|---|---|---|
| GET | `https://celab.jetfox.ovh` | `/api/v4/projects/?search={kw}&per_page=1000&page={n}` | None | `GetResults()` / `GetNumPages()` | Search workshop maps |
| GET | `https://celab.jetfox.ovh` | `/api/v4/projects/{id}/releases` | None | `FetchReleaseDetails()` | All releases: download links + preview image URLs |
| GET | `https://celab.jetfox.ovh` | `/api/v4/projects/{id}/packages` | None | `FetchImageOnly()` | Package metadata to construct image URL |
| GET | `https://celab.jetfox.ovh` | `/api/v4/projects/{id}/packages/generic/{name}/{ver}/{name}.jpg` | None | `FetchImageOnly()` (constructed) | Preview image (constructed, not enumerated) |
| GET | `{release.downloadLink}` | *(arbitrary, from API response)* | None | `RLMAPS_DownloadWorkshop()` | Download workshop map zip |
| GET | `{previewUrl}` | *(arbitrary, from API response or constructed)* | None | `DownloadPreviewImage()` | Download + cache preview image |
| GET | `https://prejump.com` | `/training-packs?page={n}` | None | `EmbeddedPackGrabber` (PS1 script) | Scrape training pack HTML page |
| GET | `https://drive.usercontent.google.com` | `/download?id=1jkl...&export=download&confirm=t` | None | `TextureDownloader` | Download workshop textures zip |
