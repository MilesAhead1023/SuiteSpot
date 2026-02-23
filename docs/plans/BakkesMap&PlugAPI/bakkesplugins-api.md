# BakkesPlugins.com API — Comprehensive Schema

Mapped via Playwright headed session with DevTools — 2026-02-23  
All 285 maps fully enumerated. Data verified against live API.

Base URL: `https://bakkesplugins.com`  
CDN Base: `https://cdn.bakkesplugins.com`  
Auth required: **None** (all read endpoints are public)  
Infrastructure: **Cloudflare** CDN + internal **GitLab** repository backend

---

## Dataset Summary (as of 2026-02-23)

| Stat | Value |
|---|---|
| Total maps | 285 |
| Maps with downloadable files | 274 |
| Maps without files (metadata only) | 11 (IDs: 68,82,121,146,152,164,171,179,180,191,192) |
| Maps with banner thumbnails | 282 |
| Maps with screenshots | 0 (field exists, always empty array) |
| Multi-version maps | 13 |
| Max versions on one map | 19 (map ID 65 — JBL Quantum Challenge) |
| Utility maps (non-playable assets) | 2 |
| Featured maps | 0 |
| Pinned maps | 0 |
| Total CDN download size | ~19.57 GB |
| Total plugins | 334 |

**Top map authors:**
| Author | Maps |
|---|---|
| Lethamyr | 153 |
| MrSwaggles | 22 |
| fractalrl | 21 |
| CommunistCowboy | 19 |
| Aceman3000 | 8 |
| Loading | 6 |
| MissMachoTV | 6 |
| JRsuhi | 6 |

**Tag distribution (maps):**
| Tag | Count |
|---|---|
| rumble | 25 |
| race | 13 |
| challenge | 11 |
| multiplayer | 11 |
| aerial | 10 |
| dribble | 10 |
| dropshot | 10 |
| hoops | 9 |
| singleplayer | 7 |
| training | 6 |
| parkour | 5 |
| soccar | 5 |
| creative | 3 |
| freestyle | 3 |
| hide-seek | 1 |

---

## Maps API

### List Maps
```
GET /api/rocket-league-maps
```

**Query Parameters:**
| Param | Type | Works | Description | Example |
|---|---|---|---|---|
| `page` | int | ✅ | Page number (1-based) | `1` |
| `pageSize` | int | ✅ | Results per page (max observed: 50+) | `16` |
| `sortBy` | string | ✅ | `downloads`, `views`, `rating`, `newest` | `downloads` |
| `search` | string | ✅ | Full-text search on name/description | `parkour` |
| `tag` | string | ❌ | Does NOT filter | — |
| `tags` | string | ❌ | Does NOT filter | — |
| `tagKey` | string | ❌ | Does NOT filter | — |
| `tagIds` | int | ❌ | Does NOT filter | — |
| `tagId` | int | ❌ | Does NOT filter | — |
| `isFeatured` | bool | ❌ | Does NOT filter | — |
| `isPinned` | bool | ❌ | Does NOT filter | — |
| `isUtility` | bool | ✅ | Filters to utility/asset maps only (returns 2) | `true` |
| `isFeatured` | bool | ❌ | Accepted but silently ignored | — |
| `isPinned` | bool | ❌ | Accepted but silently ignored | — |
| `memberId` | int | ❌ | Accepted but silently ignored | — |

> Tag filtering is **client-side only** — `tag`, `tags`, `tagKey`, `tagId`, `tagIds` are all silently ignored server-side.  
> `isUtility=true` is the **only boolean filter that works** — returns 2 maps: *Space Galaxy* and *Workshop Textures*.

**Paginated Response Envelope** (all list endpoints share this structure):
```json
{
  "items": [ ...MapSummary ],
  "totalCount": 285,
  "page": 1,
  "pageSize": 16,
  "totalPages": 18,
  "hasNextPage": true,
  "hasPreviousPage": false
}
```

**MapSummary Schema** (returned in list responses):
```jsonc
{
  "id": 138,                          // int — unique map ID
  "memberId": 120423,                 // int — author's member ID
  "name": "Ice Rings",               // string
  "shortDescription": "...",         // string — truncated description (may contain HTML)
  "bannerUrl": "https://cdn.bakkesplugins.com/uploads/<md5hash>-<filename>.<ext>",
                                     // string|null — CDN thumbnail URL
  "createdAt": "2022-06-24T20:03:53Z", // ISO 8601 UTC
  "updatedAt": "2022-07-02T04:29:00Z", // ISO 8601 UTC
  "isFeatured": false,               // bool — always false in current dataset
  "isPinned": false,                 // bool — always false in current dataset
  "isUtility": false,                // bool — non-playable asset pack if true
  "isUnmaintained": false,           // bool — always false in current dataset
  "viewCount": 45281,                // int
  "downloadCount": 38348,            // int — total across all versions
  "averageRating": 5,                // float (0–5)
  "ratingCount": 2,                  // int
  "tags": [ ...Tag ],                // array — see Tag schema
  "member": { ...MemberSummary },    // object — author info
  "latestVersionString": "1.0.0",    // string — semver of newest file
  "latestVersionFileSizeBytes": 163908818 // int — file size in bytes of latest version
}
```

---

### Get Map Detail
```
GET /api/rocket-league-maps/{id}
```

Returns the full map record including **all version files** and complete description.

**MapDetail Schema** (superset of MapSummary, additional/changed fields):
```jsonc
{
  // --- All MapSummary fields, plus: ---

  "description": "<h4>Full HTML/markdown description</h4>...", // string — full desc, may contain HTML tags and links
  "shortDescription": "...",         // string — same as description in practice (truncated in list)
  "gitLabProjectId": 152,            // int|null — internal GitLab project ID; null for ~7% of maps
  "isUnlisted": false,               // bool — always false (unlisted maps not returned by list API)
  "isPubliclyVisible": true,         // bool — always true for accessible maps
  "version": 1,                      // int — internal revision counter (not semver)
  "userRating": null,                // null — requires auth; always null for anonymous requests
  "screenshotUrls": [],              // array<string> — always empty array in current dataset
  "files": [ ...MapFile ]            // array — ALL versions, newest first. Empty array for 11 maps.
}
```

**MapFile Schema** (each entry in `files[]`):
```jsonc
{
  "id": 339,                         // int — unique file/version ID
  "mapId": 138,                      // int — parent map ID
  "fileName": "LethIceRings_1.0.0.zip", // string — original upload filename
  "edgeUrl": "https://cdn.bakkesplugins.com/uploads/<md5hash>-<filename>.zip",
                                     // string — DIRECT CDN DOWNLOAD URL (no auth required)
  "fileType": 1,                     // int — always 1 (only value observed across all 274 maps with files)
  "fileSizeBytes": 163908818,        // int — compressed zip size in bytes
  "fileHash": "54b3e0050fdd0cd5db9d683a63c0e042150dc69082a8b872db41ca1fb5038724",
                                     // string — SHA-256 hash for integrity verification
  "versionString": "1.0.0",         // string — semver label set by uploader
  "changelogMarkdown": null,         // string|null — changelog text; null in all observed records
  "downloadCount": 38348,            // int — download count for THIS specific version file
  "memberId": 120423,                // int — uploader's member ID (may differ from map owner)
  "createdAt": "2022-06-24T20:05:02Z", // ISO 8601 UTC — version upload time
  "updatedAt": "2022-06-24T20:05:02Z", // ISO 8601 UTC — always equals createdAt
  "isDeletable": true                // bool — always true for all observed files
}
```

**CDN URL Pattern:**
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-<original_filename>.zip
```
- `<md5_32hex>` — 32-character lowercase hex MD5 hash (content or upload ID)
- `<original_filename>` — original zip name including version (e.g. `LethIceRings_1.0.0.zip`)

**Banner/Thumbnail URL Pattern:**
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-<image_name>.<ext>
```
- Extensions observed: `.jpg`, `.jpeg`, `.png`, `.gif`
- Common naming: `<mapname>-preview.jpg`, `<mapname>_thumbnail.jpg`, `<mapname>.<ext>`
- 3 out of 285 maps have no banner (null)

---

### Get Map Versions
```
GET /api/rocket-league-maps/{id}/versions
```

Returns same array as `files[]` in the detail endpoint — all uploaded versions for the map, newest first.

**Version history facts:**
- 272 maps have exactly 1 version
- 13 maps have multiple versions:

| Map ID | Name | Version Count |
|---|---|---|
| 65 | JBL Quantum Challenge | 19 |
| 61 | ThePath | 15 |
| 83 | Clairefontaine | 12 |
| 95 | AsymmetricalGoalsDomination | 8 |
| 104 | Ice Rings (workshop) | 3 |
| 43 | EverOlympics | 3 |
| 59 | CreditAgricole CrossbarChallenge | 3 |
| 281 | (unnamed) | 2 |
| 163 | (unnamed) | 2 |
| 278 | (unnamed) | 2 |
| 10 | (unnamed) | 2 |
| 48 | (unnamed) | 2 |
| 178 | (unnamed) | 2 |

---

### Get Map Tags
```
GET /api/rocket-league-maps/tags
```

Returns full list of all tag definitions.

**Tag Schema:**
```jsonc
{
  "id": 3,                           // int — unique tag ID
  "key": "aerial",                   // string — URL-safe key
  "shortName": "Aerial",             // string — display label
  "longName": "Aerial Training",     // string — full name
  "helpText": "Maps for practicing aerial mechanics and flight control",
  "tagType": "map"                   // string: "map" | "utility"
}
```

**Complete Tag List (25 total):**

| id | key | shortName | tagType | Map Count |
|---|---|---|---|---|
| 3 | aerial | Aerial | map | 10 |
| 8 | challenge | Challenge | map | 11 |
| 7 | creative | Creative | map | 3 |
| 2 | dribble | Dribble | map | 10 |
| 5 | dropshot | Dropshot | map | 10 |
| 13 | freestyle | Freestyle | map | 3 |
| 4 | hide-seek | Hide & Seek | map | 1 |
| 10 | hoops | Hoops | map | 9 |
| 15 | multiplayer | Multiplayer | map | 11 |
| 1 | parkour | Parkour | map | 5 |
| 12 | race | Race | map | 13 |
| 11 | rumble | Rumble | map | 25 |
| 14 | singleplayer | Singleplayer | map | 7 |
| 6 | soccar | Soccar | map | 5 |
| 9 | training | Training | map | 6 |
| 18 | assets | Assets | utility | — |
| 24 | lighting | Lighting | utility | — |
| 17 | models | Models | utility | — |
| 21 | particles | Particles | utility | — |
| 23 | scripts | Scripts | utility | — |
| 20 | sounds | Sounds | utility | — |
| 25 | template | Template | utility | — |
| 16 | textures | Textures | utility | — |
| 19 | tools | Tools | utility | — |
| 22 | ui | UI | utility | — |

---

## Plugins API

### List Plugins
```
GET /api/plugins
```

**Query Parameters:**
| Param | Type | Works | Description | Example |
|---|---|---|---|---|
| `page` | int | ✅ | Page number (1-based) | `1` |
| `pageSize` | int | ✅ | Results per page | `16` |
| `sortBy` | string | ✅ | `downloads`, `views`, `rating`, `newest` | `downloads` |
| `search` | string | ✅ | Full-text search | `boost` |
| `tag` | string | ❌ | Does NOT filter | — |
| `tagId` | int | ❌ | Does NOT filter | — |
| `tagIds` | int | ❌ | Does NOT filter | — |
| `isPinned` | bool | ❌ | Does NOT filter | — |
| `isUnmaintained` | bool | ❌ | Does NOT filter | — |
| `memberId` | int | ❌ | Does NOT filter | — |

**PluginSummary Schema:**
```jsonc
{
  "id": 766,                          // int — unique plugin ID
  "name": "RL Discord Status",        // string
  "description": "...",              // string — full description
  "shortDescription": "Rocket League Discord Status", // string
  "homepage": "",                    // string — external homepage URL (often empty)
  "sourceCodeUrl": "https://github.com/...", // string — GitHub/source URL (often empty)
  "createdAt": "2026-02-20T10:01:31Z", // ISO 8601 UTC
  "updatedAt": "2026-02-20T20:11:19Z", // ISO 8601 UTC
  "version": 1,                      // int — internal revision counter
  "versionString": "1.1.0",         // string — semver label
  "bannerImageUrl": "https://cdn.bakkesplugins.com/uploads/<md5hash>-<filename>.<ext>",
                                     // string|null — CDN banner image URL
  "downloadCount": 209,              // int
  "viewCount": 317,                  // int
  "downloadUrl": null,               // string|null — null in list; populated in detail endpoint
  "member": { ...Member },           // object
  "tags": [ ...PluginTag ],          // array
  "averageRating": 5,                // float (0–5)
  "ratingCount": 1,                  // int
  "isPinned": false,                 // bool
  "isUnmaintained": false            // bool
}
```

---

### Get Plugin Detail
```
GET /api/plugins/{id}
```

**Additional fields over PluginSummary:**
```jsonc
{
  "downloadUrl": "https://cdn.bakkesplugins.com/uploads/<md5hash>-Bakkesplugin_<id>-public-<YYYYMMDD>-<HHMMSS>.zip",
                                     // string|null — DIRECT CDN DOWNLOAD URL for latest version
  "myStars": null                    // null — requires auth; always null for anonymous
}
```

**Plugin CDN Download URL Pattern:**
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-Bakkesplugin_<pluginId>-public-<YYYYMMDD>-<HHMMSS>.zip
```

---

### Get Plugin Versions
```
GET /api/plugins/{id}/versions
```

Returns array of submission/version objects:

**PluginVersion Schema:**
```jsonc
{
  "submissionId": 743,               // int — unique submission ID
  "pluginId": 766,                   // int — parent plugin ID
  "versionString": "1.1.0",         // string — semver label
  "submittedAt": "2026-02-20T10:01:32Z", // ISO 8601 UTC — when uploaded
  "approvedAt": "2026-02-20T20:11:19Z",  // ISO 8601 UTC — when approved by moderator
  "hasBinary": true,                 // bool — has compiled .dll binary
  "hasDebug": true,                  // bool — has debug symbols
  "binaryFileId": 12690,             // int — internal file ID for the .zip
  "debugFileId": 12691,              // int — internal file ID for debug symbols
  "status": "A",                     // string: "A" = Approved (only observed value)
  "binaryDownloadUrl": "https://cdn.bakkesplugins.com/uploads/<md5hash>-Bakkesplugin_<id>-public-<YYYYMMDD>-<HHMMSS>.zip"
                                     // string — DIRECT CDN DOWNLOAD URL for this specific version
}
```

**Plugin Tag Schema** (different from Map tags — no `tagType` field):
```jsonc
{
  "id": 1,
  "key": "quality_of_life",
  "shortName": "Quality of Life",
  "longName": "Quality of Life",
  "helpText": "Improves/aids the usage of the game itself"
}
```

**Known Plugin Tag keys:**
- `quality_of_life` — Improves/aids the usage of the game
- `freeplay` — Available in freeplay
- `game_mode` — A new gamemode not in base game
- `multiplayer` — Available in multiplayer
- `training` — Training-related
- `ranked` — For ranked play
- `custom_training` — Custom training packs

---

## Shared Object Schemas

### Member (in map responses)
```jsonc
{
  "id": 120423,                      // int — member ID
  "displayName": "Lethamyr",         // string
  "avatarUrl": null,                 // string|null — CDN avatar URL or null
  "joinDateTime": "0001-01-01T00:00:00Z", // ISO 8601 — often "0001-01-01" (legacy/unset)
  "isVerified": false,               // bool
  "roles": []                        // array — always empty for public reads
}
```

### Member (in plugin responses — more complete)
```jsonc
{
  "id": 135655,
  "displayName": "randompluginslol",
  "joinDateTime": "2026-02-20T09:50:33Z", // real date for newer accounts
  "avatarUrl": "https://cdn.bakkesplugins.com/uploads/<md5hash>-<imagename>.jpg"
}
```

---

## Endpoint Reference

| Method | Endpoint | HTTP Status | Auth | Description |
|---|---|---|---|---|
| GET | `/api/rocket-league-maps` | 200 | None | List all maps (paginated) |
| GET | `/api/rocket-league-maps/{id}` | 200 | None | Map detail + all file versions |
| GET | `/api/rocket-league-maps/{id}/versions` | 200 | None | All versions for a map |
| GET | `/api/rocket-league-maps/{id}/comments` | 200 | None | Threaded comments for a map |
| GET | `/api/rocket-league-maps/tags` | 200 | None | All 25 map tag categories |
| GET | `/api/plugins` | 200 | None | List all plugins (paginated) |
| GET | `/api/plugins/{id}` | 200 | None | Plugin detail + download URL |
| GET | `/api/plugins/{id}/versions` | 200 | None | Plugin submission/version history |
| GET | `/api/plugins/{id}/comments` | 200 | None | Flat comment list with voting for a plugin |
| POST | `/api/plugins/{id}/download` | 405 on GET | Auth? | Download trigger — requires POST, likely auth |
| GET | `/api/version` | 200 | None | Backend assembly/build version info |
| GET | `/api/plugins/tags` | 400 | — | Not available (treated as bad ID) |
| GET | `/api/rocket-league-maps/featured` | 400 | — | Not a route — `/featured` treated as bad int ID |
| GET | `/api/rocket-league-maps/{id}/reviews` | 404 | — | Does not exist |
| GET | `/api/rocket-league-maps/{id}/download` | 404 | — | Does not exist (use `edgeUrl` directly) |
| GET | `/api/plugins/{id}/reviews` | 404 | — | Does not exist |
| GET | `/api/maps` | 404 | — | Does not exist |
| GET | `/api/members/{id}` | 404 | — | Does not exist |
| GET | `/api/search` | 404 | — | Does not exist |
| GET | `/api/stats` | 404 | — | Does not exist |

---

## Comments API

### Map Comments
```
GET /api/rocket-league-maps/{id}/comments
```
Returns array of top-level comments with nested replies:

```jsonc
[
  {
    "id": 4,                            // int — comment ID
    "mapId": 138,                       // int — parent map ID
    "memberId": 123561,                 // int — author member ID
    "parentCommentId": null,            // int|null — null = top-level
    "commentText": "doesnt work for me", // string — comment body
    "createdAt": "2025-12-09T16:01:17Z", // ISO 8601 UTC
    "updatedAt": "2025-12-09T16:01:17Z", // ISO 8601 UTC
    "editedAt": null,                   // string|null — last edit time
    "isEdited": false,                  // bool
    "isDeleted": false,                 // bool
    "member": { ...Member },            // object — comment author
    "replies": [ ...MapComment ],       // array — same shape, no further nesting beyond 1 level
    "canEdit": false,                   // bool — auth-dependent; always false for anonymous
    "canDelete": false                  // bool — auth-dependent; always false for anonymous
  }
]
```

### Plugin Comments
```
GET /api/plugins/{id}/comments
```
Returns flat array — **different schema than map comments**:

```jsonc
[
  {
    "id": 204,                          // int — comment ID
    "pluginId": 766,                    // int — parent plugin ID
    "parentCommentId": null,            // int|null — null = top-level
    "body": "for it to work...",        // string — "body" not "commentText"
    "createdAt": "2026-02-21T02:39:48Z", // ISO 8601 UTC
    "updatedAt": "2026-02-21T02:39:48Z", // ISO 8601 UTC
    "member": { ...Member },            // object — comment author
    "upvotes": 0,                       // int — upvote count
    "downvotes": 0,                     // int — downvote count
    "myVote": null,                     // int|null — requires auth; null for anonymous
    "replyCount": 1                     // int — nested reply count (replies NOT returned inline)
  }
]
```

> ⚠️ **Schema differences between map and plugin comments:**
> - Maps use `commentText`; Plugins use `body`
> - Maps return inline `replies[]`; Plugins return only `replyCount`
> - Plugins have `upvotes`/`downvotes`/`myVote`; Maps do not

---

## Version / Build Info

```
GET /api/version
```

Returns backend assembly metadata (public, no auth):

```jsonc
{
  "assemblyConfiguration": "Release",          // string
  "assemblyFileVersion": "0.16.475.46458",     // string — semver
  "assemblyInformationalVersion": "0.16.475+b57ae9b", // string
  "gitCommitDate": null,                       // null — not populated
  "gitCommitId": "b57ae9b48059f500f0708e1c26a41846d0cd1dc9", // string — full git SHA
  "isPrerelease": false,                       // bool
  "isPublicRelease": false                     // bool
}
```

---

## CDN Reference

All assets served from: `https://cdn.bakkesplugins.com/uploads/`

### Map Download Files
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-<OriginalFilename>_<version>.zip
```
Examples:
- `https://cdn.bakkesplugins.com/uploads/6516c70fd371490bbbede5235a8bef9c-LethIceRings_1.0.0.zip`
- `https://cdn.bakkesplugins.com/uploads/e0b983afa1f5404a9dd66d1a70dcd8f8-JBLQuantum_1.0.18.zip`

### Map Banner Thumbnails
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-<ImageName>.<ext>
```
Examples:
- `https://cdn.bakkesplugins.com/uploads/aa314487cfc24e36a791257e46def904-preview.jpg`
- `https://cdn.bakkesplugins.com/uploads/0299bbb1e4664c71ad11a9a0f1ffd72b-preview.jpg`
Common names: `preview`, `thumbnail`, `<mapname>_thumbnail`, `<mapname>`  
Extensions: `.jpg` (most common), `.jpeg`, `.png`, `.gif`

### Plugin Download Files
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-Bakkesplugin_<pluginId>-public-<YYYYMMDD>-<HHMMSS>.zip
```
Example:
- `https://cdn.bakkesplugins.com/uploads/ee26b89377c440caa03fb187b1b0bbdb-Bakkesplugin_766-public-20260220-100229.zip`

### Plugin Banner Images
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-<OriginalFilename>.<ext>
```

### User Avatars
```
https://cdn.bakkesplugins.com/uploads/<md5_32hex>-<AvatarImageName>.<ext>
```

### CDN Notes
- All URLs are **publicly accessible** with no authentication
- The `<md5_32hex>` prefix appears to be a content or upload identifier (32-char lowercase hex)
- Files are served via Cloudflare CDN with edge caching
- No rate limiting observed on read requests

---

## Notes

- Maps are backed by an internal **GitLab** instance (`gitLabProjectId` field links to it)
- `screenshotUrls` field exists on all maps but is always an empty array — screenshots feature may be unimplemented or admin-only
- `fileType` is always `1` across all 274 maps with files — no other types observed
- `changelogMarkdown` is always `null` — changelog feature exists in schema but unused
- `isDeletable` is always `true` — likely an admin-facing field
- `userRating` / `myStars` always `null` — requires authenticated session
- `roles` on Member always empty — admin roles not exposed publicly
- **Backend is ASP.NET**, version `0.16.475`, commit `b57ae9b` (exposed via `/api/version`)
- **`/featured`, `/stats`, `/search`** as named sub-routes don't exist — the router catches them as `{id}` params (must be int), returning 400
- **Plugin IDs are not sequential from 1** — lowest observed ID is 108
- **`isUtility=true` filter works** for maps — returns only the 2 utility/asset-pack maps (*Space Galaxy*, *Workshop Textures*)
- Write operations (upload, submit plugin, rate, comment, vote) require authentication — not mapped
