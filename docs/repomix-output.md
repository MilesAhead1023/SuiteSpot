This file is a merged representation of a subset of the codebase, containing specifically included files, combined into a single document by Repomix.

# File Summary

## Purpose
This file contains a packed representation of a subset of the repository's contents that is considered the most important context.
It is designed to be easily consumable by AI systems for analysis, code review,
or other automated processes.

## File Format
The content is organized as follows:
1. This summary section
2. Repository information
3. Directory structure
4. Repository files (if enabled)
5. Multiple file entries, each consisting of:
  a. A header with the file path (## File: path/to/file)
  b. The full contents of the file in a code block

## Usage Guidelines
- This file should be treated as read-only. Any changes should be made to the
  original repository files, not this packed version.
- When processing this file, use the file path to distinguish
  between different files in the repository.
- Be aware that this file may contain sensitive information. Handle it with
  the same level of security as you would the original repository.

## Notes
- Some files may have been excluded based on .gitignore rules and Repomix's configuration
- Binary files are not included in this packed representation. Please refer to the Repository Structure section for a complete list of file paths, including binary files
- Only files matching these patterns are included: src/**
- Files matching patterns in .gitignore are excluded
- Files matching default ignore patterns are excluded
- Files are sorted by Git change count (files with more changes are at the bottom)

# Directory Structure
```
src/core/AutoLoadFeature.cpp
src/core/AutoLoadFeature.h
src/core/DefaultPacks.h
src/core/EmbeddedPackGrabber.h
src/core/LoadoutManager.cpp
src/core/LoadoutManager.h
src/core/MapList.cpp
src/core/MapList.h
src/core/MapManager.cpp
src/core/MapManager.h
src/core/PackUsageTracker.cpp
src/core/PackUsageTracker.h
src/core/SettingsSync.cpp
src/core/SettingsSync.h
src/core/TextureDownloader.cpp
src/core/TextureDownloader.h
src/core/TrainingPackManager.cpp
src/core/TrainingPackManager.h
src/core/WorkshopDownloader.cpp
src/core/WorkshopDownloader.h
src/SuiteSpot.cpp
src/SuiteSpot.h
src/ui/ConstantsUI.h
src/ui/HelpersUI.cpp
src/ui/HelpersUI.h
src/ui/LoadoutUI.cpp
src/ui/LoadoutUI.h
src/ui/SettingsUI.cpp
src/ui/SettingsUI.h
src/ui/StatusMessageUI.cpp
src/ui/StatusMessageUI.h
src/ui/TrainingPackUI.cpp
src/ui/TrainingPackUI.h
src/utils/logging.h
src/utils/ProcessUtils.h
```

# Files

## File: src/core/AutoLoadFeature.cpp
```cpp
#include "pch.h"
#include "AutoLoadFeature.h"
#include "MapList.h"
#include "SettingsSync.h"
#include "DefaultPacks.h"
#include "PackUsageTracker.h"

#include <algorithm>
#include <random>

void AutoLoadFeature::OnMatchEnded(std::shared_ptr<GameWrapper> gameWrapper,
                                   std::shared_ptr<CVarManagerWrapper> cvarManager,
                                   const std::vector<MapEntry>& freeplayMaps,
                                   const std::vector<TrainingEntry>& trainingPacks,
                                   const std::vector<WorkshopEntry>& workshopMaps,
                                   SettingsSync& settings,
                                   PackUsageTracker* usageTracker)
{
    if (!gameWrapper || !cvarManager) return;
    if (!settings.IsEnabled()) return;

    const int mapType = settings.GetMapType();
    const int delayQueueSec = settings.GetDelayQueueSec();
    const int delayFreeplaySec = settings.GetDelayFreeplaySec();
    const int delayTrainingSec = settings.GetDelayTrainingSec();
    const int delayWorkshopSec = settings.GetDelayWorkshopSec();

    std::string currentFreeplayCode = settings.GetCurrentFreeplayCode();
    std::string currentTrainingCode = settings.GetCurrentTrainingCode();
    std::string currentWorkshopPath = settings.GetCurrentWorkshopPath();

    auto safeExecute = [&](int delaySec, const std::string& cmd) {
        // Enforce a minimum delay of 0.1s to ensure the game state has settled after the match.
        // Even if the user sets 0s, we want to force a context switch out of the event stack.
        float actualDelay = (delaySec <= 0) ? 0.1f : static_cast<float>(delaySec);

        gameWrapper->SetTimeout([cvarManager, cmd](GameWrapper* gw) {
            cvarManager->executeCommand(cmd);
        }, actualDelay);
    };

    int mapLoadDelay = 0;

    if (mapType == 0) { // Freeplay
        if (currentFreeplayCode.empty()) {
            LOG("SuiteSpot: ⚠️ No freeplay map selected; skipping load.");
        } else {
            // Verify the map code exists in the list
            auto it = std::find_if(freeplayMaps.begin(), freeplayMaps.end(),
                [&](const MapEntry& e) { return e.code == currentFreeplayCode; });
            if (it != freeplayMaps.end()) {
                safeExecute(delayFreeplaySec, "load_freeplay " + currentFreeplayCode);
                mapLoadDelay = delayFreeplaySec;
                LOG("SuiteSpot: [OK] Loading freeplay map: {}", it->name);
            } else {
                LOG("SuiteSpot: [ERR] Freeplay map '{}' not found. Available maps: {}",
                    currentFreeplayCode, freeplayMaps.size());
            }
        }
    } else if (mapType == 1) { // Training
        std::string codeToLoad;
        std::string nameToLoad;

        // Single Pack Mode: use quick picks selection
        std::string targetCode = settings.GetQuickPicksSelectedCode();
        
        // If empty, try fallback to current training code (legacy)
        if (targetCode.empty()) targetCode = settings.GetCurrentTrainingCode();

        // Resolve target code
        if (!targetCode.empty()) {
            // FIX: Trust BakkesMod to handle invalid codes. Don't validate against cache.
            // This allows loading codes that aren't in our cached list (fresh/shared codes).
            codeToLoad = targetCode;

            // Try to find name in cache for logging, but don't require it
            auto it = std::find_if(trainingPacks.begin(), trainingPacks.end(),
                [&](const TrainingEntry& e) { return e.code == targetCode; });
            if (it != trainingPacks.end()) {
                nameToLoad = it->name;
            } else {
                nameToLoad = targetCode; // Use code as name if not in cache
            }
        }

        // Fallback to first Quick Pick if nothing selected or valid found
        if (codeToLoad.empty()) {
            std::vector<std::string> quickPicks;
            if (usageTracker && !usageTracker->IsFirstRun()) {
                quickPicks = usageTracker->GetTopUsedCodes(settings.GetQuickPicksCount());
            }
            
            if (quickPicks.empty()) {
                for(const auto& p : DefaultPacks::FLICKS_PICKS) quickPicks.push_back(p.code);
            }

            if (!quickPicks.empty()) {
                std::string fallbackCode = quickPicks[0];
                auto it = std::find_if(trainingPacks.begin(), trainingPacks.end(),
                    [&](const TrainingEntry& e) { return e.code == fallbackCode; });
                
                codeToLoad = fallbackCode;
                nameToLoad = (it != trainingPacks.end()) ? it->name : "Quick Pick Fallback";
                LOG("SuiteSpot: Selected pack missing, falling back to first Quick Pick: {}", nameToLoad);
            }
        }

        if (!codeToLoad.empty()) {
            // Increment usage stats for auto-loaded packs
            if (usageTracker) {
                usageTracker->IncrementLoadCount(codeToLoad);
            }

            safeExecute(delayTrainingSec, "load_training " + codeToLoad);
            mapLoadDelay = delayTrainingSec;
            LOG("SuiteSpot: Loading training pack: " + nameToLoad);
        } else {
            LOG("SuiteSpot: No training pack to load.");
        }
    } else if (mapType == 2) { // Workshop

        if (currentWorkshopPath.empty()) {
            LOG("SuiteSpot: ⚠️ No workshop map selected; skipping load.");
        } else {
            // Verify the workshop map exists in the list
            auto it = std::find_if(workshopMaps.begin(), workshopMaps.end(),
                [&](const WorkshopEntry& e) { return e.filePath == currentWorkshopPath; });
            if (it != workshopMaps.end()) {
                safeExecute(delayWorkshopSec, "load_workshop \"" + currentWorkshopPath + "\"");
                mapLoadDelay = delayWorkshopSec;
                LOG("SuiteSpot: [OK] Loading workshop map: {}", it->name);
            } else {
                LOG("SuiteSpot: [ERR] Workshop map not found: {}", currentWorkshopPath);
                LOG("SuiteSpot: 💡 Check WorkshopMapLoader plugin settings for maps folder path");
            }
        }
    }

    if (settings.IsAutoQueue()) {
        safeExecute(delayQueueSec, "queue");
        LOG("SuiteSpot: Auto-Queuing scheduled with delay: " + std::to_string(delayQueueSec) + "s.");
    }
}
```

## File: src/core/AutoLoadFeature.h
```c
#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "MapList.h"
#include "SettingsSync.h"
#include "logging.h"
#include <string>
#include <vector>
#include <memory>

/*
 * ======================================================================================
 * AUTO LOAD FEATURE: THE AUTOMATION ENGINE
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This is the component that actually "does the work" when a match ends. It is the
 * logic engine of SuiteSpot.
 * 
 * WHY IS IT HERE?
 * We need a dedicated place to decide "What happens next?" after a game.
 * Should we queue? Should we load a map? Which map? This class answers those questions.
 * 
 * HOW DOES IT WORK?
 * 1. `OnMatchEnded(...)`: This function is called by `SuiteSpot` the moment a match finishes.
 * 2. It checks `SettingsSync` to see what the user wants (e.g., "Auto Queue: ON", "Map: Workshop").
 * 3. It calculates delays (e.g., "Wait 5 seconds").
 * 4. It schedules commands using `gameWrapper->SetTimeout`.
 *    - Example: "In 5 seconds, execute command 'load_workshop my_map.upk'"
 */

class PackUsageTracker;

class AutoLoadFeature
{
public:
    // The main entry point. Called when the match ends.
    // It takes ALL the necessary data (settings, map lists) and decides what to do.
    void OnMatchEnded(std::shared_ptr<GameWrapper> gameWrapper,
        std::shared_ptr<CVarManagerWrapper> cvarManager,
        const std::vector<MapEntry>& freeplayMaps,
        const std::vector<TrainingEntry>& trainingPacks,
        const std::vector<WorkshopEntry>& workshopMaps,
        SettingsSync& settings,
        PackUsageTracker* usageTracker);
};
```

## File: src/core/DefaultPacks.h
```c
#pragma once
#include <string>
#include <vector>

struct DefaultPackData {
    std::string code;
    std::string name;
    int shotCount;
    std::string description;
};

namespace DefaultPacks {
    inline const std::vector<DefaultPackData> FLICKS_PICKS = {
        {
            "CE79-F64D-344F-5F1E",
            "Aerial Shots - Redirect",
            44,
            "This pack focuses on redirecting aerial shots, an essential offensive move to have in your arsenal. The 44 shots are designed to challenge even the most experienced players."
        },
        {
            "FA24-B2B7-2E8E-193B",
            "Ultimate Warm-Up",
            50,
            "Complete warm-up routine for all skill levels. Covers a wide range of mechanics to get you ready for ranked play."
        },
        {
            "D7F8-FD53-98D1-DAFE",
            "Backboard Therapy",
            40,
            "This training pack is designed to help players master their aerials off of the backboard. Perfect for players looking to improve their offensive capabilities."
        },
        {
            "6EB1-79B2-33B8-681C",
            "Ground Shots",
            50,
            "This training pack contains 50 shots that cover just about any possible ground shot you could see in a game. It includes a variety of angles, distances, and speeds."
        },
        {
            "5A65-4073-F310-5495",
            "Wall to Air Dribble",
            3,
            "This training pack is designed to help you improve your wall to air dribbling skills. The shots are set up to roll perfectly up the wall, allowing you to practice your setup and control."
        },
        {
            "A503-264C-A7EB-D282",
            "Musty's Speed Flip Kickoff",
            3,
            "This pack is designed to help you practice the speedflip kickoff, a technique that allows you to reach supersonic speed quickly off the kickoff."
        },
        {
            "CEB6-7AF0-9A2E-B92D",
            "Flip Reset",
            10,
            "This training pack includes a variety of shots to help you practice and master the flip reset mechanic. The shots are designed to be realistic and game-like."
        },
        {
            "6CF3-4C0B-32B4-1AC7",
            "Power Shots",
            20,
            "This pack is designed to help players improve their power and accuracy when shooting. It includes 20 shots that vary in distance and angle."
        },
        {
            "2E23-ABD5-20C6-DBD4",
            "Saves",
            50,
            "This training pack includes 50 different saves that cover a wide range of situations. From simple shots to difficult redirects, this pack will help you improve your reaction time."
        },
        {
            "CAFC-FB3E-3C0F-B8F1",
            "Double Shot Playground",
            50,
            "This pack is designed for advanced players who are looking to perfect their aerial skills and double tap shots. The shots in this pack involve using the backboard as a self-setup tool."
        }
    };
}
```

## File: src/core/EmbeddedPackGrabber.h
```c
#pragma once

// Embedded PowerShell script for training pack updates
// This script is compiled into the DLL and extracted to temp at runtime
constexpr const char* EMBEDDED_PACK_GRABBER_SCRIPT = R"PS1(# SuiteSpot Training Pack Updater
# Downloads training packs from online source and exports as JSON
#
# Requirements:
#   - PowerShell 5.0+ (included with Windows 10+)
#   - Internet connection
#   - ~5MB disk space for output JSON
#
# Performance:
#   - Total time: ~2-3 minutes
#   - Pages: 231 (10 packs per page)
#   - Rate limit: 200ms between requests (polite scraping)

param(
    [string]$OutputPath = "$env:APPDATA\bakkesmod\bakkesmod\data\SuiteSpot\TrainingSuite\training_packs.json",
    [int]$TimeoutSec = 30,
    [switch]$QuietMode = $false
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Load System.Web for HTML decoding and set TLS 1.2
Add-Type -AssemblyName System.Web
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# =====================================================================
# Helper Functions
# =====================================================================

function Write-Log {
    param(
        [string]$Message,
        [ValidateSet("Info", "Success", "Warning", "Error")][string]$Level = "Info",
        [switch]$NoNewline = $false
    )

    if ($QuietMode -and $Level -eq "Info") {
        return
    }

    $colors = @{
        "Info"    = "Cyan"
        "Success" = "Green"
        "Warning" = "Yellow"
        "Error"   = "Red"
    }

    Write-Host $Message -ForegroundColor $colors[$Level] -NoNewline:$NoNewline
}

function Invoke-PrejumpPageScrape {
    param(
        [int]$PageNumber,
        [int]$TotalPages
    )

    $url = "https://prejump.com/training-packs?page=$PageNumber"

    try {
        $response = Invoke-WebRequest -Uri $url `
            -UserAgent "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36" `
            -TimeoutSec $TimeoutSec `
            -UseBasicParsing `
            -ErrorAction Stop

        return $response.Content
    } catch {
        throw "Failed to fetch page $PageNumber : $($_.Exception.Message)"
    }
}

function Extract-PackDataFromHtml {
    param(
        [string]$HtmlContent
    )

    # Find the data-page attribute
    $pattern = 'data-page="([^"]*)"'
    $match = [regex]::Match($HtmlContent, $pattern)

    if (-not $match.Success) {
        throw "Could not find data-page attribute in HTML response"
    }

    $jsonString = $match.Groups[1].Value

    # Decode HTML entities (must use System.Web.HttpUtility for full entity support)
    $decoded = [System.Web.HttpUtility]::HtmlDecode($jsonString)

    # Parse JSON
    $pageData = $decoded | ConvertFrom-Json

    if (-not $pageData.props.packs -or -not $pageData.props.packs.data) {
        throw "Could not find packs data in page JSON"
    }

    return @{
        Packs = $pageData.props.packs.data
        TotalCount = $pageData.props.packs.meta.total
        LastPage = $pageData.props.packs.meta.last_page
        CurrentPage = $pageData.props.packs.meta.current_page
    }
}

function Normalize-TrainingPack {
    param(
        [PSObject]$Pack,
        [hashtable]$ExistingPacks = @{}
    )

    $code = $Pack.code
    $existingPack = $null
    if ($ExistingPacks.ContainsKey($code)) {
        $existingPack = $ExistingPacks[$code]
    }

    # If pack exists and was modified by user, preserve user's edits
    if ($existingPack -and $existingPack.isModified -eq $true) {
        # Keep the existing pack but update dynamic fields (likes, plays)
        return @{
            name = $existingPack.name
            code = $existingPack.code
            creator = $existingPack.creator
            creatorSlug = if ($existingPack.creatorSlug) { $existingPack.creatorSlug } else { $Pack.creatorSlug }
            difficulty = $existingPack.difficulty
            shotCount = $existingPack.shotCount
            tags = @($existingPack.tags)
            videoUrl = $existingPack.videoUrl
            gifUrl = if ($existingPack.gifUrl) { $existingPack.gifUrl } else { $Pack.gifUrl }
            staffComments = $existingPack.staffComments
            notes = $existingPack.notes
            likes = $Pack.likes  # Update dynamic stats
            plays = $Pack.plays  # Update dynamic stats
            status = $Pack.status
            source = "prejump"
            bagCategories = if ($existingPack.bagCategories) { @($existingPack.bagCategories) } else { @() }
            isModified = $true
        }
    }

    # Preserve bag categories from existing pack
    $bagCategories = @()
    if ($existingPack -and $existingPack.bagCategories) {
        $bagCategories = @($existingPack.bagCategories)
    }

    return @{
        name = $Pack.name
        code = $Pack.code
        creator = $Pack.creator
        creatorSlug = $Pack.creatorSlug
        difficulty = $Pack.difficulty
        shotCount = $Pack.shotCount
        tags = @($Pack.tags)
        videoUrl = $Pack.videoUrl
        gifUrl = $Pack.gifUrl
        staffComments = $Pack.staffComments
        notes = $Pack.notes
        likes = $Pack.likes
        plays = $Pack.plays
        status = $Pack.status
        source = "prejump"
        bagCategories = $bagCategories
        isModified = $false
    }
}

function Load-ExistingPacks {
    param(
        [string]$FilePath
    )

    $existingPacks = @{}
    $customPacks = @()

    if (-not (Test-Path $FilePath)) {
        return @{
            Packs = $existingPacks
            CustomPacks = $customPacks
        }
    }

    try {
        $content = Get-Content -Path $FilePath -Raw -Encoding UTF8
        $data = $content | ConvertFrom-Json

        if ($data.packs) {
            foreach ($pack in $data.packs) {
                $code = $pack.code
                if ($pack.source -eq "custom") {
                    # Preserve custom packs entirely
                    $customPacks += @{
                        name = $pack.name
                        code = $pack.code
                        creator = $pack.creator
                        creatorSlug = if ($pack.creatorSlug) { $pack.creatorSlug } else { "" }
                        difficulty = $pack.difficulty
                        shotCount = $pack.shotCount
                        tags = @($pack.tags)
                        videoUrl = if ($pack.videoUrl) { $pack.videoUrl } else { "" }
                        gifUrl = if ($pack.gifUrl) { $pack.gifUrl } else { "" }
                        staffComments = if ($pack.staffComments) { $pack.staffComments } else { "" }
                        notes = if ($pack.notes) { $pack.notes } else { "" }
                        likes = if ($pack.likes) { $pack.likes } else { 0 }
                        plays = if ($pack.plays) { $pack.plays } else { 0 }
                        status = if ($pack.status) { $pack.status } else { "" }
                        source = "custom"
                        bagCategories = if ($pack.bagCategories) { @($pack.bagCategories) } else { @() }
                        isModified = if ($pack.isModified) { $pack.isModified } else { $false }
                    }
                } else {
                    # Store prejump packs for reference during merge
                    $existingPacks[$code] = @{
                        name = $pack.name
                        code = $pack.code
                        creator = $pack.creator
                        creatorSlug = if ($pack.creatorSlug) { $pack.creatorSlug } else { "" }
                        difficulty = $pack.difficulty
                        shotCount = $pack.shotCount
                        tags = @($pack.tags)
                        videoUrl = if ($pack.videoUrl) { $pack.videoUrl } else { "" }
                        gifUrl = if ($pack.gifUrl) { $pack.gifUrl } else { "" }
                        staffComments = if ($pack.staffComments) { $pack.staffComments } else { "" }
                        notes = if ($pack.notes) { $pack.notes } else { "" }
                        likes = if ($pack.likes) { $pack.likes } else { 0 }
                        plays = if ($pack.plays) { $pack.plays } else { 0 }
                        status = if ($pack.status) { $pack.status } else { "" }
                        source = if ($pack.source) { $pack.source } else { "prejump" }
                        bagCategories = if ($pack.bagCategories) { @($pack.bagCategories) } else { @() }
                        isModified = if ($pack.isModified) { $pack.isModified } else { $false }
                    }
                }
            }
        }

        Write-Log "  Loaded existing file: $($existingPacks.Count) prejump packs, $($customPacks.Count) custom packs" Info

    } catch {
        Write-Log "  [WARN] Could not load existing file: $($_.Exception.Message)" Warning
    }

    return @{
        Packs = $existingPacks
        CustomPacks = $customPacks
    }
}
)PS1"
                                                     R"PS1(
# =====================================================================
# Main Scraping Logic
# =====================================================================

Write-Log "========================================================" Info
Write-Log "  SuiteSpot Training Pack Updater" Info
Write-Log "  Downloading training packs from online source..." Info
Write-Log "========================================================" Info

Write-Log "Output: $OutputPath" Info
Write-Log ""

try {
    # ===== PHASE 0: Load Existing Packs =====
    Write-Log "Phase 0: Loading existing packs for merge..." Info
    $existingData = Load-ExistingPacks -FilePath $OutputPath
    $existingPacks = $existingData.Packs
    $customPacks = $existingData.CustomPacks
    Write-Log ""

    # ===== PHASE 1: Initial Fetch =====
    Write-Log "Phase 1: Fetching initial page..." Info
    $htmlContent = Invoke-PrejumpPageScrape -PageNumber 1 -TotalPages 1

    # Extract metadata and first page of packs
    $pageInfo = Extract-PackDataFromHtml -HtmlContent $htmlContent
    Write-Log "[OK] Success" Success
    Write-Log " - Total packs available: $($pageInfo.TotalCount)" Info
    Write-Log " - Total pages to download: $($pageInfo.LastPage)" Info
    Write-Log " - First page loaded: $($pageInfo.Packs.Count) packs" Info
    Write-Log ""

    # ===== PHASE 2: Multi-Page Scraping =====
    Write-Log "Phase 2: Scraping all pages..." Info
    Write-Log ""

    [System.Collections.Generic.List[PSObject]]$allPacks = @()
    $failedPages = @()

    for ($page = 1; $page -le $pageInfo.LastPage; $page++) {
        # Show progress
        $percent = [math]::Round(($page / $pageInfo.LastPage) * 100, 1)
        $barLength = 30
        $filledBars = [math]::Floor(($page / $pageInfo.LastPage) * $barLength)
        $progressBar = "#" * $filledBars + "-" * ($barLength - $filledBars)

        Write-Host -NoNewline "`r  [$progressBar] $percent% ($page/$($pageInfo.LastPage)) "

        try {
            if ($page -eq 1) {
                # Use already-fetched content from phase 1
                $packData = $pageInfo.Packs
            } else {
                # Fetch page
                $htmlContent = Invoke-PrejumpPageScrape -PageNumber $page -TotalPages $pageInfo.LastPage
                $pageData = Extract-PackDataFromHtml -HtmlContent $htmlContent
                $packData = $pageData.Packs
            }

            # Normalize and add packs (pass existing packs for merge logic)
            foreach ($pack in $packData) {
                $normalized = Normalize-TrainingPack -Pack $pack -ExistingPacks $existingPacks
                $allPacks.Add($normalized) | Out-Null
            }

            # Polite rate limiting
            if ($page -lt $pageInfo.LastPage) {
                Start-Sleep -Milliseconds 200
            }

        } catch {
            Write-Host ""
            Write-Log "  [FAIL] Page $page failed: $($_.Exception.Message)" Warning
            $failedPages += $page
            Start-Sleep -Milliseconds 500
        }
    }

    Write-Host ""
    Write-Log "[OK] Scraping complete" Success
    Write-Log ""

    # ===== PHASE 3: Validation & Reporting =====
    Write-Log "Phase 3: Validating and preparing output..." Info

    # Deduplicate by code (keep first occurrence of each pack)
    $seenCodes = @{}
    $uniquePacks = @()
    foreach ($pack in $allPacks) {
        $code = $pack.code
        if (-not $seenCodes.ContainsKey($code)) {
            $seenCodes[$code] = $true
            $uniquePacks += $pack
        }
    }

    Write-Log "  Total packs downloaded: $($allPacks.Count)" Info
    Write-Log "  Unique packs: $($uniquePacks.Count)" Info

    if ($failedPages.Count -gt 0) {
        Write-Log "  Failed pages: $($failedPages.Count)" Warning
        $pageList = $failedPages -join ", "
        Write-Log "    Pages: $pageList" Warning
    }

    # Validate minimum success
    if ($allPacks.Count -lt 2000) {
        Write-Log "  [WARN] Expected ~2,301 packs but only got $($allPacks.Count)" Warning
        Write-Log "  Continuing with available data..." Warning
    }

    # Merge custom packs back in
    if ($customPacks.Count -gt 0) {
        Write-Log "  Preserving $($customPacks.Count) custom pack(s)" Info
    }

    Write-Log ""

    # ===== PHASE 4: Output Generation =====
    Write-Log "Phase 4: Generating output JSON..." Info

    # Combine prejump packs with preserved custom packs
    $allFinalPacks = @()
    $allFinalPacks += $uniquePacks
    $allFinalPacks += $customPacks

    # Create output structure
    $output = @{
        version = "1.1.0"
        lastUpdated = (Get-Date -Format "o")
        source = "https://prejump.com/training-packs"
        totalPacks = $allFinalPacks.Count
        packs = $allFinalPacks
    }

    # Convert to JSON with proper depth
    $json = $output | ConvertTo-Json -Depth 10 -Compress:$false

    # Ensure output directory exists
    $outputDir = Split-Path -Parent $OutputPath
    if (-not (Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir -Force -ErrorAction Stop | Out-Null
        Write-Log "  Created output directory: $outputDir" Info
    }

    # Write to file
    $json | Out-File -FilePath $OutputPath -Encoding UTF8 -Force -ErrorAction Stop

    Write-Log "[OK] JSON generated and saved" Success

    $fileSize = (Get-Item $OutputPath).Length / 1MB
    Write-Log "  File size: $([math]::Round($fileSize, 2)) MB" Info
    Write-Log "  Location: $OutputPath" Info
    Write-Log ""

    # ===== PHASE 5: Statistics =====
    Write-Log "Phase 5: Generating statistics..." Info

    # Difficulty distribution
    Write-Log ""
    Write-Log "  Packs by Difficulty:" Info
    $difficulties = $uniquePacks.difficulty | Group-Object | Sort-Object Count -Descending
    foreach ($diff in $difficulties) {
        $pct = [math]::Round(($diff.Count / $uniquePacks.Count) * 100, 1)
        $diffName = $diff.Name.PadRight(20)
        $diffCount = $diff.Count.ToString("0000")
        $logLine = "    " + $diffName + " : " + $diffCount + " [" + $pct + " pct]"
        Write-Log $logLine Info
    }

    # Tags distribution
    Write-Log ""
    Write-Log "  Tags by Category:" Info
    $allTags = @()
    foreach ($pack in $uniquePacks) {
        $allTags += $pack.tags
    }

    # Count unique tags
    $uniqueTags = $allTags | Select-Object -Unique | Measure-Object | Select-Object -ExpandProperty Count
    Write-Log "    Total unique tags: $uniqueTags" Info

    # Creator distribution (top 10)
    Write-Log ""
    Write-Log "  Top 10 Contributors:" Info
    $topCreators = $uniquePacks.creator | Group-Object | Sort-Object Count -Descending | Select-Object -First 10
    foreach ($creator in $topCreators) {
        $creatorName = $creator.Name.PadRight(20)
        Write-Log "    $creatorName : $($creator.Count) packs" Info
    }

    Write-Log ""
    Write-Log "========================================================" Success
    Write-Log "  SUCCESS - Update complete!" Success
    Write-Log "  Downloaded: $($uniquePacks.Count) packs" Success
    if ($customPacks.Count -gt 0) {
        Write-Log "  Preserved: $($customPacks.Count) custom pack(s)" Success
    }
    Write-Log "  Total: $($allFinalPacks.Count) packs in output" Success
    Write-Log "  Output: $(Split-Path -Leaf $OutputPath)" Success
    Write-Log "========================================================" Success

    exit 0

} catch {
    Write-Host "ERROR - Update failed!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}
)PS1";
```

## File: src/core/LoadoutManager.cpp
```cpp
#include "pch.h"
#include "LoadoutManager.h"
#include "logging.h"

// LoadoutManager Implementation
// 
// Purpose: Encapsulates BakkesMod LoadoutSaveWrapper operations for car loadout
// switching and management. Provides thread-safe interface using gameWrapper->Execute()
// to ensure all wrapper access happens on the game thread.
//
// BakkesMod API Flow:
//   GameWrapper::GetUserLoadoutSave() -> LoadoutSaveWrapper
//     -> GetPresets() -> ArrayWrapper<LoadoutSetWrapper>
//     -> GetEquippedLoadout() -> LoadoutSetWrapper
//     -> EquipPreset(preset) -> void (switches loadout)
//
// Thread Safety: All public methods use gameWrapper->Execute() to wrap BakkesMod
// API calls, ensuring they execute on the game thread regardless of caller context.
//
// Error Handling: All wrapper operations are null-checked. Methods return bool
// for success/failure to enable caller error handling. Failures are logged.
//
// Design Pattern: Never store wrapper references - always get fresh references
// within Execute() blocks to avoid using invalid/stale wrappers.

LoadoutManager::LoadoutManager(std::shared_ptr<GameWrapper> gameWrapper)
    : gameWrapper_(gameWrapper)
{
    // Use deferred initialization to ensure game state is ready
    if (gameWrapper_) {
        gameWrapper_->SetTimeout([this](GameWrapper* gw) {
            QueryLoadoutNamesInternal([this](size_t count) {
                 initialized_.store(true);
                 LOG("[LoadoutManager] Initialization complete, found {} loadout(s)", count);
            });
        }, 0.5f); // Small delay to ensure BakkesMod is fully loaded
    } else {
        LOG("[LoadoutManager] ERROR: GameWrapper is null during construction");
    }
}

void LoadoutManager::QueryLoadoutNamesInternal(std::function<void(size_t)> onComplete)
{
    // Internal helper that queries LoadoutSaveWrapper for all loadout preset names
    // Populates cachedLoadoutNames_ for use by GetLoadoutNames() and SwitchLoadout(index)
    
    if (!gameWrapper_) {
        LOG("[LoadoutManager] Cannot query loadouts: GameWrapper is null");
        if (onComplete) onComplete(0);
        return;
    }

    // Execute on game thread for thread-safe wrapper access
    gameWrapper_->Execute([this, onComplete](GameWrapper* gw) {
        std::vector<std::string> tempNames;
        
        try {
            auto loadoutSave = gw->GetUserLoadoutSave();
            if (loadoutSave.IsNull()) {
                LOG("[LoadoutManager] GetUserLoadoutSave() returned null");
                if (onComplete) onComplete(0);
                return;
            }

            auto presets = loadoutSave.GetPresets();
            if (presets.IsNull()) {
                LOG("[LoadoutManager] GetPresets() returned null");
                if (onComplete) onComplete(0);
                return;
            }

            // Iterate through presets and extract names
            int presetCount = presets.Count();
            // LOG("[LoadoutManager] Found {} preset(s)", presetCount); // Moved to caller or end
            
            // Reserve capacity to avoid reallocations
            tempNames.reserve(presetCount);
            
            for (int i = 0; i < presetCount; ++i) {
                auto preset = presets.Get(i);
                if (!preset.IsNull()) {
                    std::string name = preset.GetName();
                    if (!name.empty()) {
                        tempNames.push_back(name);
                    } else {
                        // LOG("[LoadoutManager] Preset at index {} has empty name", i);
                    }
                }
            }
            
            size_t newSize = tempNames.size();
            // Update cache with thread safety (inside Execute to ensure completion)
            {
                std::lock_guard<std::mutex> lock(cacheMutex_);
                cachedLoadoutNames_ = std::move(tempNames);
            }
            
            if (onComplete) {
                onComplete(newSize);
            }
        }
        catch (const std::exception& e) {
            LOG("[LoadoutManager] Exception in QueryLoadoutNamesInternal: {}", e.what());
            if (onComplete) onComplete(0);
        }
        catch (...) {
            LOG("[LoadoutManager] Unknown exception in QueryLoadoutNamesInternal");
            if (onComplete) onComplete(0);
        }
    });
}

std::vector<std::string> LoadoutManager::GetLoadoutNames()
{
    // Thread-safe access to cached loadout names
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    // Check initialization status under the same lock for consistency
    if (cachedLoadoutNames_.empty()) {
        bool isInitialized = initialized_.load();
        if (!isInitialized) {
            LOG("[LoadoutManager] GetLoadoutNames called before initialization complete");
        }
    }
    
    return cachedLoadoutNames_;
}

void LoadoutManager::GetCurrentLoadoutName(std::function<void(std::string)> onComplete)
{
    // Safely queries BakkesMod for the currently equipped loadout name
    // Returns empty string if unable to determine current loadout or if no preset is active

    if (!gameWrapper_) {
        LOG("[LoadoutManager] GetCurrentLoadoutName: GameWrapper is null");
        if (onComplete) onComplete("");
        return;
    }

    if (!onComplete) return;

    // Execute on game thread for thread-safe wrapper access
    gameWrapper_->Execute([this, onComplete](GameWrapper* gw) {
        std::string resultName = "";
        try {
            auto loadoutSave = gw->GetUserLoadoutSave();
            if (loadoutSave.IsNull()) {
                LOG("[LoadoutManager] GetCurrentLoadoutName: GetUserLoadoutSave() returned null");
                onComplete("");
                return;
            }

            // GetEquippedLoadout() returns a LoadoutSetWrapper with the currently active preset
            auto equippedLoadout = loadoutSave.GetEquippedLoadout();
            if (equippedLoadout.IsNull()) {
                LOG("[LoadoutManager] GetCurrentLoadoutName: GetEquippedLoadout() returned null");
                onComplete("");
                return;
            }

            // Get the name of the equipped loadout preset
            resultName = equippedLoadout.GetName();
            if (resultName.empty()) {
                LOG("[LoadoutManager] GetCurrentLoadoutName: Equipped loadout has empty name");
            }
            
            onComplete(resultName);
        }
        catch (const std::exception& e) {
            LOG("[LoadoutManager] Exception in GetCurrentLoadoutName: {}", e.what());
            onComplete("");
        }
        catch (...) {
            LOG("[LoadoutManager] Unknown exception in GetCurrentLoadoutName");
            onComplete("");
        }
    });
}

void LoadoutManager::SwitchLoadout(const std::string& loadoutName, std::function<void(bool)> onComplete)
{
    if (!gameWrapper_) {
        LOG("[LoadoutManager] SwitchLoadout: GameWrapper is null");
        if (onComplete) onComplete(false);
        return;
    }
    
    if (loadoutName.empty()) {
        LOG("[LoadoutManager] SwitchLoadout: Loadout name is empty");
        if (onComplete) onComplete(false);
        return;
    }

    // Capture by value to prevent reference issues
    gameWrapper_->Execute([this, loadoutName, onComplete](GameWrapper* gw) {
        bool success = false;
        try {
            auto loadoutSave = gw->GetUserLoadoutSave();
            if (loadoutSave.IsNull()) {
                LOG("[LoadoutManager] SwitchLoadout: GetUserLoadoutSave() returned null");
                if (onComplete) onComplete(false);
                return;
            }

            auto presets = loadoutSave.GetPresets();
            if (presets.IsNull()) {
                LOG("[LoadoutManager] SwitchLoadout: GetPresets() returned null");
                if (onComplete) onComplete(false);
                return;
            }

            // Search for matching preset by name
            for (int i = 0; i < presets.Count(); ++i) {
                auto preset = presets.Get(i);
                if (!preset.IsNull()) {
                    std::string presetName = preset.GetName();
                    if (presetName == loadoutName) {
                        loadoutSave.EquipPreset(preset);
                        success = true;
                        LOG("[LoadoutManager] Successfully switched to loadout: '{}'", loadoutName);
                        break;
                    }
                }
            }
            
            if (!success) {
                LOG("[LoadoutManager] SwitchLoadout: Loadout '{}' not found in presets", loadoutName);
            }
            
            if (onComplete) {
                onComplete(success);
            }
        }
        catch (const std::exception& e) {
            LOG("[LoadoutManager] Exception in SwitchLoadout: {}", e.what());
            if (onComplete) onComplete(false);
        }
        catch (...) {
            LOG("[LoadoutManager] Unknown exception in SwitchLoadout");
            if (onComplete) onComplete(false);
        }
    });
}

void LoadoutManager::SwitchLoadout(int index, std::function<void(bool)> onComplete)
{
    // Switches to the loadout at the specified index in cachedLoadoutNames_
    // Index must be valid (0 <= index < cachedLoadoutNames_.size())
    
    std::string loadoutName;
    
    // Thread-safe access to cache
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        
        if (index < 0 || index >= static_cast<int>(cachedLoadoutNames_.size())) {
            LOG("[LoadoutManager] Invalid loadout index: {} (cache size: {})", index, cachedLoadoutNames_.size());
            if (onComplete) onComplete(false);
            return;
        }
        
        loadoutName = cachedLoadoutNames_[index];
    }

    // Use the by-name method with the cached name
    SwitchLoadout(loadoutName, onComplete);
}

bool LoadoutManager::RefreshLoadoutCache()
{
    // Refreshes the cached loadout list by re-querying LoadoutSaveWrapper
    // Call this after creating or deleting loadouts externally
    // Returns true if refresh successful (at least able to query), false otherwise
    
    if (!gameWrapper_) {
        LOG("[LoadoutManager] RefreshLoadoutCache: GameWrapper is null");
        return false;
    }

    LOG("[LoadoutManager] Refreshing loadout cache...");
    
    // Re-query the loadout names
    QueryLoadoutNamesInternal([this](size_t count) {
        LOG("[LoadoutManager] Cache refresh complete, found {} loadout(s)", count);
    });
    
    // Success if we have a valid gameWrapper (QueryLoadoutNamesInternal logs its own errors)
    return true;
}

bool LoadoutManager::IsReady() const
{
    return initialized_.load();
}
```

## File: src/core/LoadoutManager.h
```c
#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>

/*
 * ======================================================================================
 * LOADOUT MANAGER: THE CAR CHANGER
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class handles changing your car preset (e.g., switching from "Octane" to "Fennec").
 * 
 * WHY IS IT HERE?
 * BakkesMod's loadout API is a bit complex and runs on the Game Thread.
 * This manager wraps all that complexity so the UI can just say "Switch to Fennec" 
 * without worrying about crashes or thread safety.
 * 
 * HOW DOES IT WORK?
 * 1. It asks the game for a list of all your presets ("Octane", "Dominus", etc.).
 * 2. It saves this list safely.
 * 3. When you click "Apply Loadout", it tells the game to equip that preset immediately.
 * 4. It uses `gameWrapper->Execute` to make sure we don't crash by touching game data
 *    from the wrong thread.
 */

class LoadoutManager
{
public:
    explicit LoadoutManager(std::shared_ptr<GameWrapper> gameWrapper);
    
    std::vector<std::string> GetLoadoutNames();
    
    void GetCurrentLoadoutName(std::function<void(std::string)> onComplete);
    
    void SwitchLoadout(const std::string& loadoutName, std::function<void(bool)> onComplete = nullptr);
    
    void SwitchLoadout(int index, std::function<void(bool)> onComplete = nullptr);
    
    bool RefreshLoadoutCache();
    
    bool IsReady() const;

private:
    std::shared_ptr<GameWrapper> gameWrapper_;
    
    std::vector<std::string> cachedLoadoutNames_;
    
    mutable std::mutex cacheMutex_;
    std::atomic<bool> initialized_{false};
    
    void QueryLoadoutNamesInternal(std::function<void(size_t)> onComplete = nullptr);
};
```

## File: src/core/MapList.cpp
```cpp
// #detailed comments: MapList data
// Purpose: Static lists of maps/training/workshop entries used by the
// UI and loading logic. These containers are intentionally defined in
// a dedicated translation unit to keep the dataset separate from
// plugin logic. Treat these vectors as read-mostly: UI code clamps
// indices and never mutates them except through explicit add/refresh
// operations which then call Save/Load helpers.
//
// DO NOT CHANGE: The string values (map codes and display names) are
// referenced by cvar persistence and by user-visible labels; renaming
// or removing entries will alter user experience and saved selections.
#include "pch.h"
#include "MapList.h"
#include "SuiteSpot.h"

std::vector<MapEntry> RLMaps = {
    { "Underwater_P","AquaDome" },
    { "Underwater_GRS_P","AquaDome (Salty Shallows)" },
    { "Park_P","Beckwith Park" },
    { "Park_Night_P","Beckwith Park (Midnight)" },
    { "Park_Snowy_P","Beckwith Park (Snowy)" },
    { "Park_Rainy_P","Beckwith Park (Stormy)" },
    { "mall_day_p","Boostfield Mall" },
    { "cs_p","Champions Field" },
    { "cs_day_p","Champions Field (Day)" },
    { "outlaw_p","Deadeye Canyon" },
    { "Outlaw_Oasis_P","Deadeye Canyon (Oasis)" },
    { "Stadium_P","DFH Stadium" },
    { "Stadium_Race_Day_p","DFH Stadium (Circuit)" },
    { "stadium_day_p","DFH Stadium (Day)" },
    { "Stadium_Winter_P","DFH Stadium (Snowy)" },
    { "Stadium_Foggy_P","DFH Stadium (Stormy)" },
    { "STADIUM_10A_P","DFH Stadium (10th Anniversary)" },
    { "woods_p","Drift Woods" },
    { "Woods_Night_P","Drift Woods (Night)" },
    { "FF_Dusk_P","Estadio Vida" },
    { "farm_p","Farmstead" },
    { "Farm_Night_P","Farmstead (Night)" },
    { "Farm_HW_P","Farmstead (Spooky)" },
    { "Farm_GRS_P","Farmstead (Pitched)" },
    { "CHN_Stadium_P","Forbidden Temple" },
    { "CHN_Stadium_Day_P","Forbidden Temple (Day)" },
    { "FNI_Stadium_P","Forbidden Temple (Fire & Ice)" },
    { "UF_Day_P","Futura Garden" },
    { "EuroStadium_P","Mannfield" },
    { "EuroStadium_Dusk_P","Mannfield (Dusk)" },
    { "EuroStadium_Night_P","Mannfield (Night)" },
    { "eurostadium_snownight_p","Mannfield (Snowy)" },
    { "EuroStadium_Rainy_P","Mannfield (Stormy)" },
    { "NeoTokyo_Standard_P","Neo Tokyo" },
    { "NeoTokyo_Toon_p","Neo Tokyo (Comic)" },
    { "NeoTokyo_Hax_P","Neo Tokyo (Hacked)" },
    { "NeoTokyo_Arcade_P","Neo Tokyo (Arcade)" },
    { "music_p","Neon Fields" },
    { "beach_P","Salty Shores" },
    { "beach_night_p","Salty Shores (Night)" },
    { "Beach_Night_GRS_P","Salty Shores (Salty Fest)" },
    { "street_p","Sovereign Heights" },
    { "arc_standard_p","Starbase ARC" },
    { "ARC_Darc_P","Starbase ARC (Aftermath)" },
    { "TrainStation_P","Urban Central" },
    { "TrainStation_Dawn_P","Urban Central (Dawn)" },
    { "TrainStation_Night_P","Urban Central (Night)" },
    { "UtopiaStadium_P","Utopia Coliseum" },
    { "UtopiaStadium_Dusk_P","Utopia Coliseum (Dusk)" },
    { "UtopiaStadium_Lux_P","Utopia Coliseum (Gilded)" },
    { "UtopiaStadium_Snow_P","Utopia Coliseum (Snowy)" },
    { "wasteland_s_p","Wasteland" },
    { "wasteland_Night_S_P","Wasteland (Night)" },
    { "Wasteland_GRS_P","Wasteland (Pitched)" },
    { "ARC_P","ARCtagon" },
    { "Wasteland_P","Badlands" },
    { "Wasteland_Night_P","Badlands (Night)" },
    { "NeoTokyo_P","Tokyo Underpass" },
    { "throwbackstadium_P","Throwback Stadium" },
    { "Labs_PillarHeat_P","Barricade" },
    { "Labs_Basin_P","Basin" },
    { "Labs_PillarWings_P","Colossus" },
    { "Labs_Corridor_P","Corridor" },
    { "Labs_Cosmic_V4_P","Cosmic" },
    { "Labs_DoubleGoal_V2_P","Double Goal" },
    { "Labs_Galleon_P","Galleon" },
    { "Labs_Galleon_Mast_P","Galleon Retro" },
    { "Labs_PillarGlass_P","Hourglass" },
    { "Labs_Holyfield_P","Loophole" },
    { "Labs_Holyfield_Space_P","Force Field" },
    { "Labs_Octagon_02_P","Octagon" },
    { "Labs_CirclePillars_P","Pillars" },
    { "Labs_Underpass_P","Underpass" },
    { "Labs_Utopia_P","Utopia Retro" },

};

std::vector<TrainingEntry> RLTraining = {
   //{"C8C8-78AF-66F2-6958", "WallReadss"}
};

std::vector<WorkshopEntry> RLWorkshop = {
    //{ "C:\\Program Files\\Epic Games\\rocketleague\\TAGame\\CookedPCConsole\\mods\\Dribble_2_Overhaul\\map.upk, mapnae" }
};
```

## File: src/core/MapList.h
```c
#pragma once
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <memory>
#include "bakkesmod/plugin/bakkesmodplugin.h"

// Freeplay maps
struct MapEntry
{
    std::string code;
    std::string name;
};
extern std::vector<MapEntry> RLMaps;

// Training packs
struct TrainingEntry
{
    std::string code;
    std::string name;

    // Pack metadata
    std::string creator;           // Creator's display name
    std::string creatorSlug;       // Creator's username (for linking)
    std::string difficulty;        // Bronze, Gold, Platinum, Diamond, Champion, Supersonic Legend
    std::vector<std::string> tags; // Array of tags
    int shotCount = 0;             // Number of shots
    std::string staffComments;     // Staff description
    std::string notes;             // Creator's notes
    std::string videoUrl;          // Optional YouTube tutorial link
    std::string gifUrl;            // Optional imgur preview clip (mp4)
    int likes = 0;                 // Engagement metric
    int plays = 0;                 // Engagement metric
    int status = 1;                // Pack status (1 = active)

    // Unified system fields
    std::string source = "prejump"; // "prejump" or "custom"
    bool isModified = false;        // Track if user edited a scraped pack

    // Thumbnail cache (loaded lazily when pack is selected in browser)
    std::shared_ptr<ImageWrapper> thumbnailImage;
    bool isThumbnailRequested = false; // Prevents duplicate HTTP fetches
};
extern std::vector<TrainingEntry> RLTraining;

// Workshop maps
struct WorkshopEntry
{
    std::string filePath;                       // UPK file path
    std::string name;                           // Display name
    std::string author;                         // Map author (from JSON)
    std::string description;                    // Map description (from JSON)
    std::filesystem::path folder;               // Map folder path
    std::filesystem::path previewPath;          // Preview image path (.jfif, .jpg, .png)
    std::shared_ptr<ImageWrapper> previewImage; // Loaded image
    bool isImageLoaded = false;
};
extern std::vector<WorkshopEntry> RLWorkshop;
```

## File: src/core/MapManager.cpp
```cpp
#include "pch.h"
#include "MapManager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_set>

#include "IMGUI/json.hpp"

namespace {
std::string Trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string StripQuotes(const std::string& value)
{
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string ExpandEnvAndHome(const std::string& input)
{
    std::string expanded;
    expanded.reserve(input.size());

    for (size_t i = 0; i < input.size();) {
        if (input[i] == '%') {
            const auto end = input.find('%', i + 1);
            if (end != std::string::npos) {
                const auto varName = input.substr(i + 1, end - i - 1);
                if (!varName.empty()) {
                    if (const char* val = std::getenv(varName.c_str())) {
                        expanded.append(val);
                    }
                }
                i = end + 1;
                continue;
            }
        }
        expanded.push_back(input[i]);
        ++i;
    }

    if (!expanded.empty() && expanded[0] == '~') {
        if (const char* home = std::getenv("USERPROFILE")) {
            expanded.replace(0, 1, home);
        }
    }

    return expanded;
}

int CaseInsensitiveCompare(const std::string& a, const std::string& b)
{
    const size_t len = std::min(a.size(), b.size());
    for (size_t i = 0; i < len; i++) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) return (ca < cb) ? -1 : 1;
    }
    if (a.size() == b.size()) return 0;
    return (a.size() < b.size()) ? -1 : 1;
}

} // namespace

MapManager::MapManager() {}

std::filesystem::path MapManager::GetDataRoot() const
{
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return std::filesystem::path();
    return std::filesystem::path(appdata) / "bakkesmod" / "bakkesmod" / "data";
}

std::filesystem::path MapManager::GetSuiteSpotRoot() const
{
    return GetDataRoot() / "SuiteSpot";
}

std::filesystem::path MapManager::GetSuiteTrainingDir() const
{
    return GetSuiteSpotRoot() / "TrainingSuite";
}

std::filesystem::path MapManager::GetWorkshopCacheDir() const
{
    return GetSuiteSpotRoot() / "Workshop";
}

std::filesystem::path MapManager::GetTexturesDir() const
{
    return GetWorkshopCacheDir(); // Same as workshop cache
}

std::filesystem::path MapManager::GetTrainingPacksPath() const
{
    return GetSuiteTrainingDir() / "training_packs.json";
}

std::filesystem::path MapManager::GetWorkshopLoaderConfigPath() const
{
    return GetDataRoot() / "WorkshopMapLoader" / "workshopmaploader.cfg";
}

std::filesystem::path MapManager::ResolveConfiguredWorkshopRoot() const
{
    const auto cfg = GetWorkshopLoaderConfigPath();
    std::ifstream in(cfg);
    if (!in.is_open()) {
        return {};
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const auto keyPos = trimmed.find("MapsFolderPath");
        if (keyPos == std::string::npos) {
            continue;
        }

        const auto eqPos = trimmed.find('=', keyPos);
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string value = trimmed.substr(eqPos + 1);
        value = StripQuotes(Trim(value));
        value = ExpandEnvAndHome(value);

        if (value.empty()) {
            continue;
        }

        std::error_code ec;
        std::filesystem::path candidate(value);
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
            return candidate;
        }

        LOG("SuiteSpot: Configured workshop path not found: " + value);
    }

    return {};
}

void MapManager::EnsureDataDirectories() const
{
    std::error_code ec;
    auto root = GetDataRoot();
    if (!root.empty()) std::filesystem::create_directories(root, ec);
    ec.clear();
    std::filesystem::create_directories(GetSuiteTrainingDir(), ec);
}

bool MapManager::LoadWorkshopMetadata(const std::filesystem::path& jsonPath, std::string& outTitle,
                                      std::string& outAuthor, std::string& outDescription) const
{
    std::error_code ec;
    if (!std::filesystem::exists(jsonPath, ec)) return false;

    try {
        std::ifstream file(jsonPath);
        if (!file.is_open()) return false;

        // Read entire file content
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Sanitize content: replace control characters that break JSON parsing
        for (char& c : content) {
            if (c >= 0 && c < 32 && c != '\t' && c != '\n' && c != '\r') {
                c = ' '; // Replace control chars with space
            }
        }

        // Try parsing with allow_exceptions=false first
        nlohmann::json j = nlohmann::json::parse(content, nullptr, false);

        if (j.is_discarded()) {
            // JSON parsing failed - try to extract fields manually as fallback
            // Look for "Title":"value" pattern
            auto extractField = [&content](const std::string& fieldName) -> std::string {
                std::string pattern = "\"" + fieldName + "\":\"";
                size_t start = content.find(pattern);
                if (start == std::string::npos) return "";
                start += pattern.length();
                size_t end = content.find("\"", start);
                if (end == std::string::npos) return "";
                return content.substr(start, end - start);
            };

            outTitle = extractField("Title");
            outAuthor = extractField("Author");
            // Skip description for malformed files - it's usually what contains the bad data
            outDescription = "";

            return !outTitle.empty(); // Consider success if we at least got the title
        }

        if (j.contains("Title") && j["Title"].is_string()) {
            outTitle = j["Title"].get<std::string>();
        }
        if (j.contains("Author") && j["Author"].is_string()) {
            outAuthor = j["Author"].get<std::string>();
        }
        if (j.contains("Description") && j["Description"].is_string()) {
            outDescription = j["Description"].get<std::string>();
        }
        return true;
    } catch (const std::exception&) {
        // Silently fail for malformed JSON - don't spam the log
        return false;
    }
}

std::filesystem::path MapManager::FindPreviewImage(const std::filesystem::path& folder) const
{
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec)) return {};

    // Check common preview image extensions
    static const std::vector<std::string> extensions = {".jfif", ".jpg", ".jpeg", ".png", ".gif", ".webp"};

    for (const auto& file : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!file.is_regular_file()) continue;

        std::string ext = file.path().extension().string();
        // Convert to lowercase for comparison
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

        for (const auto& validExt : extensions) {
            if (ext == validExt) {
                return file.path();
            }
        }
    }
    return {};
}

void MapManager::DiscoverWorkshopInDir(const std::filesystem::path& dir, std::vector<WorkshopEntry>& workshop) const
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_directory()) continue;

        std::string foundMapFile;
        std::filesystem::path foundJsonFile;

        // Scan for UPK and JSON files
        for (const auto& file : std::filesystem::directory_iterator(entry.path(), ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!file.is_regular_file()) continue;

            const auto& path = file.path();
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

            if (ext == ".upk" && foundMapFile.empty()) {
                foundMapFile = path.string();
            } else if (ext == ".json" && foundJsonFile.empty()) {
                foundJsonFile = path;
            }
        }

        if (!foundMapFile.empty()) {
            WorkshopEntry workshopEntry;
            workshopEntry.filePath = foundMapFile;
            workshopEntry.folder = entry.path();
            workshopEntry.name = entry.path().filename().string();

            // Try to load metadata from JSON
            std::string title, author, description;
            if (!foundJsonFile.empty() && LoadWorkshopMetadata(foundJsonFile, title, author, description)) {
                if (!title.empty()) workshopEntry.name = title;
                workshopEntry.author = author;
                workshopEntry.description = description;
            }

            // Find preview image
            workshopEntry.previewPath = FindPreviewImage(entry.path());

            workshop.push_back(workshopEntry);
        }
    }
}

void MapManager::LoadWorkshopMaps(std::vector<WorkshopEntry>& workshop, int& currentWorkshopIndex)
{
    workshop.clear();

    std::vector<std::filesystem::path> roots;
    std::unordered_set<std::string> seenRoots;

    // Helper to add root only if not already seen
    auto addRoot = [&](const std::filesystem::path& path) {
        if (path.empty()) return;
        std::error_code ec;
        std::string canonical;
        if (std::filesystem::exists(path, ec)) {
            canonical = std::filesystem::canonical(path, ec).string();
        }
        if (canonical.empty()) {
            canonical = path.string();
        }
        // Normalize to lowercase for comparison on Windows
        std::string lower = canonical;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (seenRoots.insert(lower).second) {
            roots.push_back(path);
        }
    };

    if (const auto configured = ResolveConfiguredWorkshopRoot(); !configured.empty()) {
        addRoot(configured);
    }

    const char* progFiles = std::getenv("ProgramFiles");
    const char* progFilesX86 = std::getenv("ProgramFiles(x86)");

    if (progFiles) {
        addRoot(std::filesystem::path(progFiles) / "Epic Games" / "rocketleague" / "TAGame" / "CookedPCConsole" / "mods");
    }
    if (progFilesX86) {
        addRoot(std::filesystem::path(progFilesX86) / "Steam" / "steamapps" / "common" / "rocketleague" / "TAGame" /
                "CookedPCConsole" / "mods");
    }

    for (const auto& root : roots) {
        DiscoverWorkshopInDir(root, workshop);
    }

    std::unordered_set<std::string> seen;
    std::vector<WorkshopEntry> unique;
    unique.reserve(workshop.size());
    for (const auto& entry : workshop) {
        if (seen.insert(entry.filePath).second) {
            unique.push_back(entry);
        }
    }
    workshop.swap(unique);

    std::sort(workshop.begin(), workshop.end(), [](const WorkshopEntry& lhs, const WorkshopEntry& rhs) {
        const int cmp = CaseInsensitiveCompare(lhs.name, rhs.name);
        if (cmp == 0) {
            return lhs.filePath < rhs.filePath;
        }
        return cmp < 0;
    });

    if (workshop.empty()) {
        currentWorkshopIndex = 0;
    } else {
        currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, static_cast<int>(workshop.size() - 1));
    }
}

// ===== HOTKEY CYCLING METHODS =====

void MapManager::CycleMapMode(bool forward)
{
    // Cycle through map modes: Freeplay (0) → Training (1) → Workshop (2) → Freeplay (0)
    if (forward) {
        currentMapModeIndex = (currentMapModeIndex + 1) % 3;
    } else {
        currentMapModeIndex = (currentMapModeIndex - 1 + 3) % 3;
    }
    // Caller (SuiteSpot hotkey handler) will update the CVar with currentMapModeIndex
}

void MapManager::CycleMap(bool forward)
{
    // Cycle through maps within the current mode
    // Get the current list based on current map mode
    if (currentMapModeIndex == 0 && !RLMaps.empty()) {
        // Freeplay maps
        if (forward) {
            currentFreeplayIndex = (currentFreeplayIndex + 1) % RLMaps.size();
        } else {
            currentFreeplayIndex = (currentFreeplayIndex - 1 + RLMaps.size()) % RLMaps.size();
        }
    } else if (currentMapModeIndex == 1 && !RLTraining.empty()) {
        // Training packs
        if (forward) {
            currentTrainingIndex = (currentTrainingIndex + 1) % RLTraining.size();
        } else {
            currentTrainingIndex = (currentTrainingIndex - 1 + RLTraining.size()) % RLTraining.size();
        }
    } else if (currentMapModeIndex == 2 && !RLWorkshop.empty()) {
        // Workshop maps
        if (forward) {
            currentWorkshopIndex = (currentWorkshopIndex + 1) % RLWorkshop.size();
        } else {
            currentWorkshopIndex = (currentWorkshopIndex - 1 + RLWorkshop.size()) % RLWorkshop.size();
        }
    }
    // Caller (SuiteSpot hotkey handler) will update the appropriate CVar
}

std::string MapManager::GetCurrentFreeplayCode() const
{
    if (!RLMaps.empty() && currentFreeplayIndex >= 0 && currentFreeplayIndex < (int)RLMaps.size())
        return RLMaps[currentFreeplayIndex].code;
    return "";
}

std::string MapManager::GetCurrentTrainingCode() const
{
    if (!RLTraining.empty() && currentTrainingIndex >= 0 && currentTrainingIndex < (int)RLTraining.size())
        return RLTraining[currentTrainingIndex].code;
    return "";
}

std::string MapManager::GetCurrentWorkshopPath() const
{
    if (!RLWorkshop.empty() && currentWorkshopIndex >= 0 && currentWorkshopIndex < (int)RLWorkshop.size())
        return RLWorkshop[currentWorkshopIndex].filePath;
    return "";
}

std::string MapManager::GetCurrentFreeplayName() const
{
    if (!RLMaps.empty() && currentFreeplayIndex >= 0 && currentFreeplayIndex < (int)RLMaps.size())
        return RLMaps[currentFreeplayIndex].name;
    return "";
}

std::string MapManager::GetCurrentTrainingName() const
{
    if (!RLTraining.empty() && currentTrainingIndex >= 0 && currentTrainingIndex < (int)RLTraining.size())
        return RLTraining[currentTrainingIndex].name;
    return "";
}

std::string MapManager::GetCurrentWorkshopName() const
{
    if (!RLWorkshop.empty() && currentWorkshopIndex >= 0 && currentWorkshopIndex < (int)RLWorkshop.size())
        return RLWorkshop[currentWorkshopIndex].name;
    return "";
}
```

## File: src/core/MapManager.h
```c
#pragma once
#include "MapList.h"
#include "logging.h"
#include <filesystem>
#include <string>
#include <vector>

/*
 * ======================================================================================
 * MAP MANAGER: THE MAP FINDER
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class is responsible for finding maps on your computer.
 * 
 * WHY IS IT HERE?
 * Rocket League (and BakkesMod) don't automatically know where all your Workshop maps are,
 * especially if you downloaded them manually or use a custom folder. We need to scan
 * the disk to find them.
 * 
 * HOW DOES IT WORK?
 * 1. `DiscoverWorkshopInDir()`: You give it a folder (like "C:\MyMaps"), and it looks for
 *    files ending in `.upk` or `.udk`.
 * 2. It creates a list of these maps (`WorkshopEntry`) so the UI can display them.
 * 3. It helps other parts of the plugin figure out where the "Data" folder is.
 */

class MapManager
{
  public:
    MapManager();

    // Finds the main "Data" folder where we save our stuff
    std::filesystem::path GetDataRoot() const;

    // Finds the specific folder for SuiteSpot data
    std::filesystem::path GetSuiteSpotRoot() const;
    std::filesystem::path GetSuiteTrainingDir() const;

    // Workshop-specific paths
    std::filesystem::path GetWorkshopCacheDir() const;
    std::filesystem::path GetTexturesDir() const;

    // Training pack path
    std::filesystem::path GetTrainingPacksPath() const;

    // Makes sure these folders actually exist (creates them if missing)
    void EnsureDataDirectories() const;

    // Workshop Helpers
    std::filesystem::path GetWorkshopLoaderConfigPath() const;
    std::filesystem::path ResolveConfiguredWorkshopRoot() const;

    // The big scanner: Finds maps in a folder and adds them to the list
    void DiscoverWorkshopInDir(const std::filesystem::path& dir, std::vector<WorkshopEntry>& outList) const;

    // Refreshes the list of maps
    void LoadWorkshopMaps(std::vector<WorkshopEntry>& outList, int& currentIndex);

    // Hotkey cycling methods (used by hotkey system)
    // Note: These methods update internal indices and should be followed by
    // SettingsSync CVar updates to persist the selection
    void CycleMapMode(bool forward); // Cycle between Freeplay/Training/Workshop
    void CycleMap(bool forward);     // Cycle within current map type

    // Get current indices for cycling
    int GetCurrentMapModeIndex() const { return currentMapModeIndex; }
    int GetCurrentFreeplayIndex() const { return currentFreeplayIndex; }
    int GetCurrentTrainingIndex() const { return currentTrainingIndex; }
    int GetCurrentWorkshopIndex() const { return currentWorkshopIndex; }

    // Get the actual code/path for the current selection (use after CycleMap)
    std::string GetCurrentFreeplayCode() const;
    std::string GetCurrentTrainingCode() const;
    std::string GetCurrentWorkshopPath() const;
    std::string GetCurrentFreeplayName() const;
    std::string GetCurrentTrainingName() const;
    std::string GetCurrentWorkshopName() const;

  private:
    std::filesystem::path dataRoot;

    // Cycling indices
    int currentMapModeIndex = 0; // 0=Freeplay, 1=Training, 2=Workshop
    int currentFreeplayIndex = 0;
    int currentTrainingIndex = 0;
    int currentWorkshopIndex = 0;

    // Parse workshop JSON metadata file
    bool LoadWorkshopMetadata(const std::filesystem::path& jsonPath, std::string& outTitle, std::string& outAuthor,
                              std::string& outDescription) const;

    // Find preview image in workshop folder (.jfif, .jpg, .png)
    std::filesystem::path FindPreviewImage(const std::filesystem::path& folder) const;
};
```

## File: src/core/PackUsageTracker.cpp
```cpp
#include "pch.h"
#include "PackUsageTracker.h"
#include "logging.h"
#include "IMGUI/json.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>

using json = nlohmann::json;

PackUsageTracker::PackUsageTracker(const std::filesystem::path& statsFilePath) : filePath(statsFilePath)
{
    LoadStats();
}

void PackUsageTracker::LoadStats()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!std::filesystem::exists(filePath)) {
        isFirstRun = true;
        return;
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return;

        json j;
        file >> j;

        if (j.contains("stats") && j["stats"].is_array()) {
            for (const auto& item : j["stats"]) {
                PackUsageStats s;
                s.code = item.value("code", "");
                s.loadCount = item.value("loadCount", 0);
                s.lastLoadedTimestamp = item.value("lastLoadedTimestamp", 0LL);

                if (!s.code.empty()) {
                    stats[s.code] = s;
                }
            }
            isFirstRun = stats.empty();
        }
    } catch (const std::exception& e) {
        LOG("Failed to load pack usage stats: {}", e.what());
    }
}

void PackUsageTracker::SaveStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        json j;
        j["version"] = "1.0.0";
        j["stats"] = json::array();

        for (const auto& [code, s] : stats) {
            j["stats"].push_back(
                {{"code", s.code}, {"loadCount", s.loadCount}, {"lastLoadedTimestamp", s.lastLoadedTimestamp}});
        }

        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << j.dump(4);
        }
    } catch (const std::exception& e) {
        LOG("Failed to save pack usage stats: {}", e.what());
    }
}

void PackUsageTracker::IncrementLoadCount(const std::string& packCode)
{
    if (packCode.empty()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& s = stats[packCode];
        s.code = packCode;
        s.loadCount++;
        s.lastLoadedTimestamp =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        isFirstRun = false;
    }

    SaveStats();
}

int PackUsageTracker::GetLoadCount(const std::string& packCode) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stats.find(packCode);
    return (it != stats.end()) ? it->second.loadCount : 0;
}

int64_t PackUsageTracker::GetLastPlayedTimestamp(const std::string& packCode) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stats.find(packCode);
    return (it != stats.end()) ? it->second.lastLoadedTimestamp : 0;
}

std::vector<std::string> PackUsageTracker::GetTopUsedCodes(int count) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PackUsageStats> allStats;
    for (const auto& [code, s] : stats) {
        allStats.push_back(s);
    }

    // Sort by load count (desc), then by last loaded timestamp (desc)
    std::sort(allStats.begin(), allStats.end(), [](const PackUsageStats& a, const PackUsageStats& b) {
        if (a.loadCount != b.loadCount) {
            return a.loadCount > b.loadCount;
        }
        return a.lastLoadedTimestamp > b.lastLoadedTimestamp;
    });

    std::vector<std::string> result;
    for (int i = 0; i < std::min(count, (int)allStats.size()); ++i) {
        result.push_back(allStats[i].code);
    }
    return result;
}
```

## File: src/core/PackUsageTracker.h
```c
#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <mutex>
#include <cstdint>

struct PackUsageStats
{
    std::string code;
    int loadCount = 0;
    int64_t lastLoadedTimestamp = 0;
};

class PackUsageTracker
{
  public:
    explicit PackUsageTracker(const std::filesystem::path& statsFilePath);

    void LoadStats();
    void SaveStats() const;
    void IncrementLoadCount(const std::string& packCode);
    int GetLoadCount(const std::string& packCode) const;
    int64_t GetLastPlayedTimestamp(const std::string& packCode) const;
    std::vector<std::string> GetTopUsedCodes(int count) const;
    bool IsFirstRun() const { return isFirstRun; }

  private:
    std::filesystem::path filePath;
    std::map<std::string, PackUsageStats> stats;
    bool isFirstRun = true;
    mutable std::mutex mutex_;
};
```

## File: src/core/SettingsSync.cpp
```cpp
#include "pch.h"
#include "SettingsSync.h"

#include <algorithm>

void SettingsSync::RegisterAllCVars(const std::shared_ptr<CVarManagerWrapper>& cvarManager)
{
    if (!cvarManager) return;

    cvarManager->registerCvar("suitespot_enabled", "0", "Enable SuiteSpot", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { enabled = cvar.getBoolValue(); });

    cvarManager
        ->registerCvar("suitespot_map_type", "0", "Map type: 0=Freeplay, 1=Training, 2=Workshop", true, true, 0, true, 2)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { mapType = cvar.getIntValue(); });

    cvarManager->registerCvar("suitespot_auto_queue", "0", "Enable auto-queuing after map load", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { autoQueue = cvar.getBoolValue(); });

    cvarManager
        ->registerCvar("suitespot_fix_training_gamespeed", "1",
                       "Keep in-game training speed synced with BM in training playlists", true, true, 0, true, 1)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { trainingGameSpeedFixEnabled = cvar.getBoolValue(); });

    cvarManager
        ->registerCvar("suitespot_quickpicks_list_type", "0", "List type: 0=Flicks Picks, 1=Your Favorites", true, true,
                       0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { quickPicksListType = cvar.getIntValue(); });

    cvarManager
        ->registerCvar("suitespot_quickpicks_count", "10", "Number of quick picks to show", true, true, 5, true, 15)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { quickPicksCount = cvar.getIntValue(); });

    cvarManager->registerCvar("suitespot_quickpicks_selected", "", "Selected quick pick pack code", true)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { quickPicksSelected = cvar.getStringValue(); });

    cvarManager
        ->registerCvar("suitespot_delay_queue_sec", "0", "Delay before queuing (seconds)", true, true, 0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayQueueSec = std::max(0, cvar.getIntValue()); });

    cvarManager
        ->registerCvar("suitespot_delay_freeplay_sec", "0", "Delay before loading freeplay map (seconds)", true, true,
                       0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayFreeplaySec = std::max(0, cvar.getIntValue()); });

    cvarManager
        ->registerCvar("suitespot_delay_training_sec", "0", "Delay before loading training map (seconds)", true, true,
                       0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayTrainingSec = std::max(0, cvar.getIntValue()); });

    cvarManager
        ->registerCvar("suitespot_delay_workshop_sec", "0", "Delay before loading workshop map (seconds)", true, true,
                       0, true, 300)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { delayWorkshopSec = std::max(0, cvar.getIntValue()); });

    cvarManager->registerCvar("suitespot_current_freeplay_code", "", "Currently selected freeplay map code", true)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { currentFreeplayCode = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_current_training_code", "", "Currently selected training pack code", true)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { currentTrainingCode = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_current_workshop_path", "", "Currently selected workshop map path", true)
        .addOnValueChanged(
            [this](std::string oldValue, CVarWrapper cvar) { currentWorkshopPath = cvar.getStringValue(); });

    cvarManager->registerCvar("ss_training_maps", "", "Stored training maps", true, false, 0, false, 0);

    // Hotkey bindings: dual-key required. Key1 = trigger, Key2 = must be held.
    // Both are UE3 key name strings (e.g. "J", "F3", "LeftAlt", "XboxTypeS_DPad_Up").
    // All detection is handled by SuiteSpot's HandleKeyPress hook — no setBind used.

    cvarManager->registerCvar("suitespot_hotkey_map_mode_fwd_key", "", "Key 1 for cycle map mode forward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyMapModeFwdKey = cvar.getStringValue(); });
    cvarManager->registerCvar("suitespot_hotkey_map_mode_fwd_key2", "", "Key 2 (held) for cycle map mode forward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyMapModeFwdKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_map_mode_bk_key", "", "Key 1 for cycle map mode backward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyMapModeBkKey = cvar.getStringValue(); });
    cvarManager->registerCvar("suitespot_hotkey_map_mode_bk_key2", "", "Key 2 (held) for cycle map mode backward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyMapModeBkKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_cycle_map_fwd_key", "", "Key 1 for cycle map forward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyCycleMapFwdKey = cvar.getStringValue(); });
    cvarManager->registerCvar("suitespot_hotkey_cycle_map_fwd_key2", "", "Key 2 (held) for cycle map forward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyCycleMapFwdKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_cycle_map_bk_key", "", "Key 1 for cycle map backward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyCycleMapBkKey = cvar.getStringValue(); });
    cvarManager->registerCvar("suitespot_hotkey_cycle_map_bk_key2", "", "Key 2 (held) for cycle map backward", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyCycleMapBkKey2 = cvar.getStringValue(); });

    cvarManager->registerCvar("suitespot_hotkey_load_now_key", "", "Key 1 for load current map immediately", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyLoadNowKey = cvar.getStringValue(); });
    cvarManager->registerCvar("suitespot_hotkey_load_now_key2", "", "Key 2 (held) for load now", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) { hotkeyLoadNowKey2 = cvar.getStringValue(); });

    // Note: CVars auto-initialize to defaults from registerCvar() above
    // The addOnValueChanged callbacks will sync values if user has saved config
    // No need to redundantly set values here
}

void SettingsSync::SetCurrentFreeplayCode(const std::string& code)
{
    currentFreeplayCode = code;
}

void SettingsSync::SetCurrentTrainingCode(const std::string& code)
{
    currentTrainingCode = code;
}

void SettingsSync::SetQuickPicksSelected(const std::string& code)
{
    quickPicksSelected = code;
}

void SettingsSync::SetCurrentWorkshopPath(const std::string& path)
{
    currentWorkshopPath = path;
}
```

## File: src/core/SettingsSync.h
```c
#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <memory>

/*
 * ======================================================================================
 * SETTINGS SYNC: THE CONFIGURATION MANAGER
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class manages all the "CVars" (Console Variables). CVars are how BakkesMod stores
 * settings so they are remembered when you restart the game.
 * 
 * WHY IS IT HERE?
 * We need a single, safe place to read and write settings. If we scattered this logic
 * everywhere, we might misspell a setting name or use the wrong default value.
 * 
 * HOW DOES IT WORK?
 * 1. `RegisterAllCVars()`: Called once at startup. It tells BakkesMod: "Hey, I have a setting called 
 *    'suitespot_enabled', please remember it for me."
 * 2. It keeps a local copy of every setting (e.g., `bool enabled`) for fast access.
 * 3. When BakkesMod says "The user changed this setting in the console," this class 
 *    automatically updates its local copy.
 */

class SettingsSync
{
  public:
    // Tells BakkesMod about all our settings
    void RegisterAllCVars(const std::shared_ptr<CVarManagerWrapper>& cvarManager);

    // Getters: Fast, safe ways to ask "Is this feature on?"
    bool IsEnabled() const { return enabled; }
    int GetMapType() const { return mapType; }
    bool IsAutoQueue() const { return autoQueue; }
    bool IsTrainingGameSpeedFixEnabled() const { return trainingGameSpeedFixEnabled; }
    int GetQuickPicksListType() const { return quickPicksListType; }
    int GetQuickPicksCount() const { return quickPicksCount; }
    std::string GetQuickPicksSelected() const { return quickPicksSelected; }

    // Delay getters (How long to wait?)
    int GetDelayQueueSec() const { return delayQueueSec; }
    int GetDelayFreeplaySec() const { return delayFreeplaySec; }
    int GetDelayTrainingSec() const { return delayTrainingSec; }
    int GetDelayWorkshopSec() const { return delayWorkshopSec; }

    // Selection getters (Which map/pack is selected?)
    std::string GetCurrentFreeplayCode() const { return currentFreeplayCode; }
    std::string GetCurrentTrainingCode() const { return currentTrainingCode; }
    std::string GetQuickPicksSelectedCode() const { return quickPicksSelected; }
    std::string GetCurrentWorkshopPath() const { return currentWorkshopPath; }

    // Hotkey getters — key1 = trigger, key2 = required held modifier (both must be set)
    std::string GetHotkeyMapModeFwdKey1() const { return hotkeyMapModeFwdKey; }
    std::string GetHotkeyMapModeFwdKey2() const { return hotkeyMapModeFwdKey2; }
    std::string GetHotkeyMapModeBkKey1() const { return hotkeyMapModeBkKey; }
    std::string GetHotkeyMapModeBkKey2() const { return hotkeyMapModeBkKey2; }
    std::string GetHotkeyCycleMapFwdKey1() const { return hotkeyCycleMapFwdKey; }
    std::string GetHotkeyCycleMapFwdKey2() const { return hotkeyCycleMapFwdKey2; }
    std::string GetHotkeyCycleMapBkKey1() const { return hotkeyCycleMapBkKey; }
    std::string GetHotkeyCycleMapBkKey2() const { return hotkeyCycleMapBkKey2; }
    std::string GetHotkeyLoadNowKey1() const { return hotkeyLoadNowKey; }
    std::string GetHotkeyLoadNowKey2() const { return hotkeyLoadNowKey2; }

    // Setters: Update the local value (used when loading data)
    void SetCurrentFreeplayCode(const std::string& code);
    void SetCurrentTrainingCode(const std::string& code);
    void SetQuickPicksSelected(const std::string& code);
    void SetCurrentWorkshopPath(const std::string& path);

  private:
    // Local copies of settings for fast access
    bool enabled = false;
    int mapType = 0; // 0=Freeplay, 1=Training, 2=Workshop
    bool autoQueue = false;
    bool trainingGameSpeedFixEnabled = true;
    int quickPicksListType = 0; // 0=Flicks Picks, 1=Your Favorites
    int quickPicksCount = 10;
    std::string quickPicksSelected = "";

    int delayQueueSec = 0;
    int delayFreeplaySec = 0;
    int delayTrainingSec = 0;
    int delayWorkshopSec = 0;

    std::string currentFreeplayCode; // Freeplay map code (e.g., "beckwith_park_p")
    std::string currentTrainingCode; // Training pack code (e.g., "XXXX-XXXX-XXXX-XXXX")
    std::string currentWorkshopPath; // Workshop map path (e.g., "C:/path/to/map.udk")

    // Hotkey bindings: key1 (UE3 string, trigger key) + key2 (UE3 string, optional held combo partner)
    std::string hotkeyMapModeFwdKey;
    std::string hotkeyMapModeFwdKey2;
    std::string hotkeyMapModeBkKey;
    std::string hotkeyMapModeBkKey2;
    std::string hotkeyCycleMapFwdKey;
    std::string hotkeyCycleMapFwdKey2;
    std::string hotkeyCycleMapBkKey;
    std::string hotkeyCycleMapBkKey2;
    std::string hotkeyLoadNowKey;
    std::string hotkeyLoadNowKey2;
};
```

## File: src/core/TrainingPackManager.cpp
```cpp
#include "bakkesmod/wrappers/gfx/GfxDataTrainingWrapper.h"
#include "bakkesmod/wrappers/GameEvent/SaveData/TrainingEditorSaveDataWrapper.h"
#include "pch.h"
#include "TrainingPackManager.h"
#include "EmbeddedPackGrabber.h"
#include "ProcessUtils.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <thread>

void TrainingPackManager::LoadPacksFromFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath)) {
        LOG("SuiteSpot: Pack cache file not found: {}", filePath.string());
        {
            std::lock_guard<std::mutex> lock(packMutex);
            RLTraining.clear();
            packCount = 0;
        }
        lastUpdated = "Never";
        return;
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG("SuiteSpot: Failed to open Pack cache file");
            return;
        }

        nlohmann::json jsonData;
        file >> jsonData;
        file.close();

        std::lock_guard<std::mutex> lock(packMutex);
        RLTraining.clear();

        if (!jsonData.contains("packs") || !jsonData["packs"].is_array()) {
            LOG("SuiteSpot: Invalid Pack cache file format - missing 'packs' array");
            return;
        }

        for (const auto& pack : jsonData["packs"]) {
            TrainingEntry entry;

            if (pack.contains("code") && pack["code"].is_string()) {
                entry.code = pack["code"].get<std::string>();
            }
            if (pack.contains("name") && pack["name"].is_string()) {
                entry.name = pack["name"].get<std::string>();
            }

            if (entry.code.empty() || entry.name.empty()) {
                continue;
            }

            if (pack.contains("creator") && pack["creator"].is_string()) {
                entry.creator = pack["creator"].get<std::string>();
            }
            if (pack.contains("creatorSlug") && pack["creatorSlug"].is_string()) {
                entry.creatorSlug = pack["creatorSlug"].get<std::string>();
            }
            if (pack.contains("difficulty") && pack["difficulty"].is_string()) {
                entry.difficulty = pack["difficulty"].get<std::string>();
            }
            if (pack.contains("shotCount") && pack["shotCount"].is_number()) {
                entry.shotCount = pack["shotCount"].get<int>();
            }
            if (pack.contains("staffComments") && pack["staffComments"].is_string()) {
                entry.staffComments = pack["staffComments"].get<std::string>();
            }
            if (pack.contains("notes") && pack["notes"].is_string()) {
                entry.notes = pack["notes"].get<std::string>();
            }
            if (pack.contains("videoUrl") && pack["videoUrl"].is_string()) {
                entry.videoUrl = pack["videoUrl"].get<std::string>();
            }
            if (pack.contains("gifUrl") && pack["gifUrl"].is_string()) {
                entry.gifUrl = pack["gifUrl"].get<std::string>();
            }
            if (pack.contains("likes") && pack["likes"].is_number()) {
                entry.likes = pack["likes"].get<int>();
            }
            if (pack.contains("plays") && pack["plays"].is_number()) {
                entry.plays = pack["plays"].get<int>();
            }
            if (pack.contains("status") && pack["status"].is_number()) {
                entry.status = pack["status"].get<int>();
            }

            if (pack.contains("tags") && pack["tags"].is_array()) {
                for (const auto& tag : pack["tags"]) {
                    if (tag.is_string()) {
                        entry.tags.push_back(tag.get<std::string>());
                    }
                }
            }

            // Unified system fields
            if (pack.contains("source") && pack["source"].is_string()) {
                entry.source = pack["source"].get<std::string>();
            } else {
                entry.source = "prejump"; // Default for backward compatibility
            }

            // Bag categories removed - skip loading bagCategories and orderInBag

            if (pack.contains("isModified") && pack["isModified"].is_boolean()) {
                entry.isModified = pack["isModified"].get<bool>();
            }

            RLTraining.push_back(entry);
        }

        // Sort RLTraining alphabetically by name
        std::sort(RLTraining.begin(), RLTraining.end(), [](const TrainingEntry& a, const TrainingEntry& b) {
            // Case-insensitive comparison for names
            std::string nameA = a.name;
            std::string nameB = b.name;
            std::transform(nameA.begin(), nameA.end(), nameA.begin(), [](unsigned char c) { return std::tolower(c); });
            std::transform(nameB.begin(), nameB.end(), nameB.begin(), [](unsigned char c) { return std::tolower(c); });
            return nameA < nameB;
        });

        packCount = static_cast<int>(RLTraining.size());
        lastUpdated = GetLastUpdatedTime(filePath);
        currentFilePath = filePath;

        LOG("SuiteSpot: Loaded {} training packs from file", packCount);

    } catch (const std::exception& e) {
        LOG("SuiteSpot: Error loading training packs: {}", std::string(e.what()));
        {
            std::lock_guard<std::mutex> lock(packMutex);
            RLTraining.clear();
            packCount = 0;
        }
    }
}

bool TrainingPackManager::IsCacheStale(const std::filesystem::path& filePath) const
{
    if (!std::filesystem::exists(filePath)) {
        return true;
    }

    try {
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        auto now = std::filesystem::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWriteTime);

        return age.count() > 168;
    } catch (...) {
        return true;
    }
}

std::string TrainingPackManager::GetLastUpdatedTime(const std::filesystem::path& filePath) const
{
    if (!std::filesystem::exists(filePath)) {
        return "Never";
    }

    try {
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lastWriteTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto tt = std::chrono::system_clock::to_time_t(sctp);

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M UTC");
        return oss.str();
    } catch (...) {
        return "Unknown";
    }
}

void TrainingPackManager::UpdateTrainingPackList(const std::filesystem::path& outputPath,
                                                 const std::shared_ptr<GameWrapper>& gameWrapper)
{
    if (scrapingInProgress) {
        LOG("SuiteSpot: Training pack update already in progress");
        return;
    }

    if (!gameWrapper) {
        LOG("SuiteSpot: GameWrapper unavailable for training pack update");
        return;
    }

    scrapingInProgress = true;

    LOG("SuiteSpot: Training pack updater starting");
    LOG("SuiteSpot: Output path: {}", outputPath.string());

    // Join previous update thread if still running
    if (updateThread.joinable()) updateThread.join();

    // Launch update in background thread to avoid blocking game thread
    updateThread = std::thread([this, outputPath]() {
        try {
            // Get temp directory for script extraction
            std::filesystem::path tempDir = std::filesystem::temp_directory_path();
            auto tempScript = tempDir / "SuitePackGrabber_temp.ps1";
            auto logFile = outputPath.parent_path() / "PackGrabber.log";

            // Write embedded script to temp file
            {
                std::ofstream tempFile(tempScript, std::ios::binary);
                if (!tempFile.is_open()) {
                    LOG("SuiteSpot: Failed to create temp script file: {}", tempScript.string());
                    scrapingInProgress = false;
                    return;
                }
                tempFile.write(EMBEDDED_PACK_GRABBER_SCRIPT, std::string_view(EMBEDDED_PACK_GRABBER_SCRIPT).length());
                tempFile.close();
            }

            std::string outStr = outputPath.string();

            LOG("SuiteSpot: Training pack updater started");

            std::string scriptArgs = "-OutputPath \"" + outStr + "\"";
            int result = Utils::RunPowerShellScript(tempScript.string(), scriptArgs, logFile.string());

            LOG("SuiteSpot: Training pack updater returned: {}", result);

            // Try to read error log if it exists
            if (std::filesystem::exists(logFile)) {
                std::ifstream log(logFile);
                std::string line;
                LOG("SuiteSpot: PackGrabber output:");
                while (std::getline(log, line)) {
                    LOG("  {}", line);
                }
            }

            if (result == 0) {
                LOG("SuiteSpot: Training pack update completed successfully");
                LoadPacksFromFile(outputPath);
                lastUpdated = GetLastUpdatedTime(outputPath);
            } else {
                LOG("SuiteSpot: Training pack updater returned non-zero exit code: {}", result);
            }

            // Clean up temp script
            try {
                if (std::filesystem::exists(tempScript)) {
                    std::filesystem::remove(tempScript);
                    LOG("SuiteSpot: Cleaned up temporary script file");
                }
            } catch (const std::exception& e) {
                LOG("SuiteSpot: Warning - failed to clean up temp script: {}", std::string(e.what()));
            }

        } catch (const std::exception& e) {
            LOG("SuiteSpot: Training pack updater error: {}", std::string(e.what()));
        }

        scrapingInProgress = false;
    });
}

void TrainingPackManager::FilterAndSortPacks(const std::string& searchText, const std::string& difficultyFilter,
                                             const std::string& tagFilter, int minShots, int maxShots, bool videoFilter,
                                             int sortColumn, bool sortAscending, std::vector<TrainingEntry>& out) const
{
    std::lock_guard<std::mutex> lock(packMutex);
    out.clear();

    std::string searchLower(searchText);
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& pack : RLTraining) {
        // Video filter — skip packs without video or gif preview
        if (videoFilter && pack.videoUrl.empty() && pack.gifUrl.empty()) {
            continue;
        }

        if (!searchLower.empty()) {
            // Search only by NAME or CODE
            bool matchesSearch = false;

            // 1. Search by name (case-insensitive, partial match)
            std::string nameLower = pack.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (nameLower.find(searchLower) != std::string::npos) {
                matchesSearch = true;
            }

            // 2. Search by code (supports both formatted "XXXX-XXXX-XXXX-XXXX" and unformatted 16-char)
            if (!matchesSearch) {
                // Remove all dashes from both search term and pack code for comparison
                std::string codeLower = pack.code;
                std::string searchNoDashes = searchLower;
                std::transform(codeLower.begin(), codeLower.end(), codeLower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                // Strip dashes from both strings
                codeLower.erase(std::remove(codeLower.begin(), codeLower.end(), '-'), codeLower.end());
                searchNoDashes.erase(std::remove(searchNoDashes.begin(), searchNoDashes.end(), '-'),
                                     searchNoDashes.end());

                // Match if search term is contained in code (allows partial code search)
                if (codeLower.find(searchNoDashes) != std::string::npos) {
                    matchesSearch = true;
                }
            }

            if (!matchesSearch) {
                continue;
            }
        }

        if (difficultyFilter != "All") {
            if (difficultyFilter == "Unranked") {
                // Match "Unranked" filter against all "no difficulty" values
                if (!pack.difficulty.empty() && pack.difficulty != "Unknown" && pack.difficulty != "All" &&
                    pack.difficulty != "Unranked") {
                    continue;
                }
            } else {
                // Standard strict match for ranked tiers
                if (pack.difficulty != difficultyFilter) {
                    continue;
                }
            }
        }

        if (!tagFilter.empty()) {
            bool hasTag = false;
            for (const auto& tag : pack.tags) {
                if (tag == tagFilter) {
                    hasTag = true;
                    break;
                }
            }
            if (!hasTag) continue;
        }

        // Unknown shot count (null in JSON → 0): only show when min is 0 (user hasn't restricted range)
        if (pack.shotCount == 0) {
            if (minShots > 0) continue;
        } else {
            if (pack.shotCount < minShots || pack.shotCount > maxShots) continue;
        }

        out.push_back(pack);
    }

    // Case-insensitive string comparison helper
    auto caseInsensitiveCompare = [](const std::string& a, const std::string& b) -> int {
        std::string lowerA = a;
        std::string lowerB = b;
        std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), [](unsigned char c) { return std::tolower(c); });
        return lowerA.compare(lowerB);
    };

    // Difficulty ranking helper (lower rank = easier)
    auto getDifficultyRank = [](const std::string& difficulty) -> int {
        std::string diffLower = difficulty;
        std::transform(diffLower.begin(), diffLower.end(), diffLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (diffLower.empty() || diffLower == "unknown" || diffLower == "all" || diffLower == "unranked") return 0;
        if (diffLower == "bronze") return 1;
        if (diffLower == "silver") return 2;
        if (diffLower == "gold") return 3;
        if (diffLower == "platinum") return 4;
        if (diffLower == "diamond") return 5;
        if (diffLower == "champion") return 6;
        if (diffLower == "grand champion") return 7;
        if (diffLower == "supersonic legend") return 8;
        return 0; // Unknown difficulties default to unranked
    };

    std::sort(out.begin(), out.end(),
              [sortColumn, sortAscending, &caseInsensitiveCompare, &getDifficultyRank](const TrainingEntry& a,
                                                                                       const TrainingEntry& b) {
                  int cmp = 0;
                  switch (sortColumn) {
                      case 0: // Name
                          cmp = caseInsensitiveCompare(a.name, b.name);
                          break;
                      case 1: // Creator
                          cmp = caseInsensitiveCompare(a.creator, b.creator);
                          break;
                      case 2: // Difficulty
                      {
                          int rankA = getDifficultyRank(a.difficulty);
                          int rankB = getDifficultyRank(b.difficulty);
                          cmp = (rankA < rankB) ? -1 : (rankA > rankB) ? 1 : 0;
                      } break;
                      case 3: // Shots
                          cmp = (a.shotCount < b.shotCount) ? -1 : (a.shotCount > b.shotCount) ? 1 : 0;
                          break;
                      case 4: // Likes
                          cmp = (a.likes < b.likes) ? -1 : (a.likes > b.likes) ? 1 : 0;
                          break;
                      case 5: // Plays
                          cmp = (a.plays < b.plays) ? -1 : (a.plays > b.plays) ? 1 : 0;
                          break;
                  }
                  return sortAscending ? (cmp < 0) : (cmp > 0);
              });
}

void TrainingPackManager::BuildAvailableTags(std::vector<std::string>& out) const
{
    std::lock_guard<std::mutex> lock(packMutex);
    std::set<std::string> uniqueTags;
    for (const auto& pack : RLTraining) {
        for (const auto& tag : pack.tags) {
            uniqueTags.insert(tag);
        }
    }

    out.clear();
    out.push_back("All Tags");
    for (const auto& tag : uniqueTags) {
        out.push_back(tag);
    }
}

void TrainingPackManager::SavePacksToFile(const std::filesystem::path& filePath)
{
    try {
        std::lock_guard<std::mutex> lock(packMutex);
        nlohmann::json output;
        output["version"] = "1.0.0";

        // Get current time in ISO format
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
        output["lastUpdated"] = oss.str();

        output["source"] = "https://prejump.com/training-packs";
        output["totalPacks"] = RLTraining.size();

        nlohmann::json packsArray = nlohmann::json::array();
        for (const auto& pack : RLTraining) {
            nlohmann::json p;
            p["name"] = pack.name;
            p["code"] = pack.code;
            p["creator"] = pack.creator;
            p["creatorSlug"] = pack.creatorSlug;
            p["difficulty"] = pack.difficulty;
            p["shotCount"] = pack.shotCount;
            p["tags"] = pack.tags;
            p["videoUrl"] = pack.videoUrl;
            p["gifUrl"] = pack.gifUrl;
            p["staffComments"] = pack.staffComments;
            p["notes"] = pack.notes;
            p["likes"] = pack.likes;
            p["plays"] = pack.plays;
            p["status"] = pack.status;

            // Unified system fields
            p["source"] = pack.source;
            // Bag categories removed - don't save bagCategories or orderInBag
            p["isModified"] = pack.isModified;

            packsArray.push_back(p);
        }
        output["packs"] = packsArray;

        // Ensure directory exists
        auto parentDir = filePath.parent_path();
        if (!std::filesystem::exists(parentDir)) {
            std::filesystem::create_directories(parentDir);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG("SuiteSpot: Failed to open file for writing: {}", filePath.string());
            return;
        }

        file << output.dump(2); // Pretty print with 2-space indent
        file.close();

        currentFilePath = filePath;
        lastUpdated = GetLastUpdatedTime(filePath);
        LOG("SuiteSpot: Saved {} packs to file", RLTraining.size());

    } catch (const std::exception& e) {
        LOG("SuiteSpot: Error saving packs: {}", std::string(e.what()));
    }
}

bool TrainingPackManager::AddCustomPack(const TrainingEntry& pack)
{
    {
        std::lock_guard<std::mutex> lock(packMutex);
        // Check for duplicate code
        for (const auto& existing : RLTraining) {
            if (existing.code == pack.code) {
                LOG("SuiteSpot: Pack with code {} already exists", pack.code);
                return false;
            }
        }

        TrainingEntry newPack = pack;
        newPack.source = "custom";
        RLTraining.push_back(newPack);

        // Sort RLTraining alphabetically by name
        std::sort(RLTraining.begin(), RLTraining.end(), [](const TrainingEntry& a, const TrainingEntry& b) {
            std::string nameA = a.name;
            std::string nameB = b.name;
            std::transform(nameA.begin(), nameA.end(), nameA.begin(), [](unsigned char c) { return std::tolower(c); });
            std::transform(nameB.begin(), nameB.end(), nameB.begin(), [](unsigned char c) { return std::tolower(c); });
            return nameA < nameB;
        });

        packCount = static_cast<int>(RLTraining.size());
        LOG("SuiteSpot: Added custom pack: {}", pack.name);
        // Lock releases here before SavePacksToFile
    }

    // Auto-save if we have a file path (outside the lock)
    if (!currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
    }
    return true;
}

bool TrainingPackManager::UpdatePack(const std::string& code, const TrainingEntry& updatedPack)
{
    {
        std::lock_guard<std::mutex> lock(packMutex);
        for (auto& pack : RLTraining) {
            if (pack.code == code) {
                // Preserve source and update isModified
                std::string originalSource = pack.source;
                pack = updatedPack;
                pack.source = originalSource;

                // Mark as modified if it was a prejump pack
                if (originalSource == "prejump") {
                    pack.isModified = true;
                }

                // Sort RLTraining alphabetically by name
                std::sort(RLTraining.begin(), RLTraining.end(), [](const TrainingEntry& a, const TrainingEntry& b) {
                    std::string nameA = a.name;
                    std::string nameB = b.name;
                    std::transform(nameA.begin(), nameA.end(), nameA.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    std::transform(nameB.begin(), nameB.end(), nameB.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    return nameA < nameB;
                });

                LOG("SuiteSpot: Updated pack: {}", pack.name);
                // Lock releases here before SavePacksToFile
            }
        }
    }

    // Auto-save outside the lock
    if (!currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
        return true;
    }
    return false;
}

bool TrainingPackManager::DeletePack(const std::string& code)
{
    std::string name;
    {
        std::lock_guard<std::mutex> lock(packMutex);
        auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
                               [&code](const TrainingEntry& p) { return p.code == code; });

        if (it != RLTraining.end()) {
            name = it->name;
            RLTraining.erase(it);
            packCount = static_cast<int>(RLTraining.size());

            LOG("SuiteSpot: Deleted pack: {}", name);
        } else {
            return false;
        }
    }

    // Auto-save outside the lock
    if (!currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
    }
    return true;
}

// ============================================================================
// CATEGORIZED BAG SYSTEM
// ============================================================================

void TrainingPackManager::HealPack(const std::string& code, int shots)
{
    if (code.empty() || shots <= 0) {
        LOG("SuiteSpot: HealPack called with invalid data - code: {}, shots: {}", code, shots);
        return;
    }

    bool needsSave = false;
    bool packFound = false;
    {
        std::lock_guard<std::mutex> lock(packMutex);
        for (auto& pack : RLTraining) {
            if (pack.code == code) {
                packFound = true;
                // Heal if shot count is missing or incorrect
                if (pack.shotCount <= 0 || pack.shotCount != shots) {
                    int oldCount = pack.shotCount;
                    pack.shotCount = shots;
                    needsSave = true;
                    LOG("SuiteSpot: Healed pack '{}' ({}): {} -> {} shots", pack.name, code, oldCount, shots);
                } else {
                    LOG("SuiteSpot: Pack '{}' ({}) already has correct shot count: {}", pack.name, code, shots);
                }
                break;
            }
        }
    }

    if (!packFound) {
        LOG("SuiteSpot: HealPack - Pack not found in database: {}", code);
    }

    if (needsSave && !currentFilePath.empty()) {
        SavePacksToFile(currentFilePath);
        LOG("SuiteSpot: Saved healed pack data to: {}", currentFilePath.string());
    }
}

std::optional<TrainingEntry> TrainingPackManager::GetPackByCode(const std::string& code) const
{
    std::lock_guard<std::mutex> lock(packMutex);
    for (const auto& pack : RLTraining) {
        if (pack.code == code) {
            return pack;
        }
    }
    return std::nullopt;
}
```

## File: src/core/TrainingPackManager.h
```c
#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "MapList.h"
#include "logging.h"
#include "IMGUI/json.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>

/*
 * ======================================================================================
 * TRAINING PACK MANAGER: THE LIBRARY
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This is the librarian of the plugin. It keeps track of every training pack you have,
 * whether they came from the internet or you added them yourself.
 *
 * WHY IS IT HERE?
 * Managing 2000+ training packs requires a lot of organization. We need to save them
 * to a file, load them back up, search them, and handle the "Shuffle Bag" logic.
 *
 * HOW DOES IT WORK?
 * 1. `LoadPacksFromFile()`: Reads `training_packs.json` and turns it into a list of `TrainingEntry` objects.
 * 2. `UpdateTrainingPackList()`: Runs a PowerShell script to download the latest packs from the web.
 * 3. `FilterAndSortPacks()`: When you type in the search bar, this function decides which packs to show.
 * 4. `Categorized Bags`: Organize packs into categories (Defense, Offense, etc.) for structured training rotations.
 */

class TrainingPackManager
{
  public:
    ~TrainingPackManager()
    {
        if (updateThread.joinable()) updateThread.join();
    }
    // Core data operations
    void LoadPacksFromFile(const std::filesystem::path& filePath);
    bool IsCacheStale(const std::filesystem::path& filePath) const;
    std::string GetLastUpdatedTime(const std::filesystem::path& filePath) const;

    // Downloads fresh data from online source
    void UpdateTrainingPackList(const std::filesystem::path& outputPath, const std::shared_ptr<GameWrapper>& gameWrapper);

    // Search and Sort logic
    void FilterAndSortPacks(const std::string& searchText, const std::string& difficultyFilter,
                            const std::string& tagFilter, int minShots, int maxShots, bool videoFilter, int sortColumn,
                            bool sortAscending, std::vector<TrainingEntry>& out) const;

    // Helper for the UI tag filter
    void BuildAvailableTags(std::vector<std::string>& out) const;

    // Management (CRUD) operations
    bool AddCustomPack(const TrainingEntry& pack);
    bool UpdatePack(const std::string& code, const TrainingEntry& updatedPack);
    void HealPack(const std::string& code, int shots);
    bool DeletePack(const std::string& code);

    // Accessors
    const std::vector<TrainingEntry>& GetPacks() const { return RLTraining; }
    int GetPackCount() const { return packCount; }
    std::string GetLastUpdated() const { return lastUpdated; }
    bool IsScrapingInProgress() const { return scrapingInProgress; }
    std::optional<TrainingEntry> GetPackByCode(const std::string& code) const;

  private:
    void SavePacksToFile(const std::filesystem::path& filePath);

    std::vector<TrainingEntry> RLTraining;
    mutable std::mutex packMutex; // Protects RLTraining from concurrent access
    int packCount = 0;
    std::string lastUpdated = "Never";
    bool scrapingInProgress = false;
    std::filesystem::path currentFilePath;
    std::thread updateThread;
};
```

## File: src/core/WorkshopDownloader.cpp
```cpp
#include "pch.h"
#include "WorkshopDownloader.h"
#include "ProcessUtils.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {
// Safe JSON string extraction with type checking
std::string SafeGetString(const nlohmann::json& j, const std::string& key, const std::string& defaultVal = "")
{
    if (!j.contains(key)) return defaultVal;
    if (j[key].is_null()) return defaultVal;
    if (j[key].is_string()) return j[key].get<std::string>();
    if (j[key].is_number_integer()) return std::to_string(j[key].get<int64_t>());
    if (j[key].is_number()) return std::to_string(j[key].get<double>());
    return j[key].dump(); // Fallback: convert to string representation
}

std::string SafeGetNestedString(const nlohmann::json& j, const std::vector<std::string>& keys,
                                const std::string& defaultVal = "")
{
    const nlohmann::json* current = &j;
    for (const auto& key : keys) {
        if (!current->contains(key)) return defaultVal;
        current = &(*current)[key];
    }
    return current->is_string() ? current->get<std::string>() : defaultVal;
}

// Detect image extension from URL
std::string GetImageExtension(const std::string& url)
{
    std::string urlLower = url;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);

    if (urlLower.find(".png") != std::string::npos) return ".png";
    if (urlLower.find(".jpg") != std::string::npos) return ".jpg";
    if (urlLower.find(".jpeg") != std::string::npos) return ".jpeg";
    if (urlLower.find(".webp") != std::string::npos) return ".webp";
    if (urlLower.find(".gif") != std::string::npos) return ".gif";

    return ".jfif"; // Default fallback
}
} // namespace

WorkshopDownloader::WorkshopDownloader(std::shared_ptr<GameWrapper> gw) : gameWrapper(gw)
{
    BakkesmodPath = gw->GetDataFolder().string() + "\\";
    IfNoPreviewImagePath = BakkesmodPath + "SuiteSpot\\Workshop\\NoPreview.jpg";
}

WorkshopDownloader::~WorkshopDownloader()
{
    StopSearch();
    if (searchThread.joinable()) {
        searchThread.join();
    }
    // Wait for all in-flight FetchMapDetail/DownloadPreviewImage callbacks to fire
    // before the destructor returns and the DLL can be unmapped. Without this, the
    // detached threads can call back into unmapped code and crash.
    int waited = 0;
    while (completedResults < expectedResults && waited < 5000) {
        Sleep(10);
        waited += 10;
    }
}

void WorkshopDownloader::GetResults(std::string keyWord)
{
    // Prevent new search if one is already in progress
    bool expectedSearching = false;
    if (!isSearching.compare_exchange_strong(expectedSearching, true)) {
        LOG("Search already in progress, ignoring new search request");
        return;
    }

    // Reset state
    stopRequested = false;
    completedRequests = 0;

    // Increment generation
    int currentGeneration = ++searchGeneration;

    // Clear list immediately under lock
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        mapResults.clear();
        listVersion++;
    }

    // Join previous thread if active
    if (searchThread.joinable()) {
        searchThread.join();
    }

    std::weak_ptr<WorkshopDownloader> weak_self = shared_from_this();

    searchThread = std::thread([weak_self, keyWord, currentGeneration]() {
        auto self = weak_self.lock();
        if (!self) return;

        std::string searchUrl = self->apiBase + "?search=" + keyWord + "&pageSize=500&page=1";

        CurlRequest req;
        req.url = searchUrl;

        HttpWrapper::SendCurlRequest(req, [weak_self, currentGeneration](int code, std::string result) {
            auto self = weak_self.lock();
            if (!self) return;

            if (self->stopRequested || self->searchGeneration != currentGeneration) {
                self->isSearching = false;
                return;
            }

            if (code != 200) {
                LOG("[ERR] Workshop search failed with HTTP code {}", code);
                self->isSearching = false;
                self->SearchErrorBool = true;
                self->SearchErrorText = "Search failed: HTTP " + std::to_string(code);
                return;
            }

            self->SearchErrorBool = false;
            self->SearchErrorText.clear();

            try {
                nlohmann::json j = nlohmann::json::parse(result);

                if (!j.is_object() || !j.contains("items") || !j["items"].is_array()) {
                    LOG("[ERR] Unexpected response format from BakkesPlugins API");
                    self->isSearching = false;
                    self->SearchErrorBool = true;
                    self->SearchErrorText = "Unexpected API response format";
                    return;
                }

                auto& items = j["items"];
                self->mapsFound = j.contains("totalCount") ? j["totalCount"].get<int>() : (int)items.size();
                LOG("Workshop search found {} maps", self->mapsFound.load());

                if (items.empty()) {
                    self->isSearching = false;
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(self->resultsMutex);
                    for (const auto& item : items) {
                        if (!item.contains("id") || !item.contains("name")) continue;

                        WorkshopMap map;
                        map.ID = std::to_string(item["id"].get<int>());
                        map.Name = SafeGetString(item, "name", "Unknown Map");
                        map.Description = SafeGetString(item, "shortDescription", "");
                        map.PreviewUrl = SafeGetString(item, "bannerUrl", "");
                        map.Author = SafeGetNestedString(item, {"member", "displayName"}, "Unknown");
                        if (item.contains("latestVersionFileSizeBytes") && !item["latestVersionFileSizeBytes"].is_null()) {
                            map.Size = std::to_string(item["latestVersionFileSizeBytes"].get<int64_t>());
                        }
                        self->mapResults.push_back(map);
                    }
                    self->listVersion++;
                    LOG("Map list populated: {} items", self->mapResults.size());
                }

                // Kick off per-map detail fetch to get download link + full description
                int totalMaps = (int)self->mapResults.size();
                self->expectedResults = totalMaps;
                self->completedResults = 0;

                for (int i = 0; i < totalMaps; ++i) {
                    std::thread t(&WorkshopDownloader::FetchMapDetail, self.get(), i, currentGeneration);
                    t.detach();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                while (self->completedResults < self->expectedResults) {
                    if (self->stopRequested || self->searchGeneration != currentGeneration) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                self->isSearching = false;
                LOG("[OK] Workshop search complete: {}/{} maps loaded", self->completedResults.load(),
                    self->expectedResults.load());

            } catch (const std::exception& e) {
                LOG("[ERR] Workshop search JSON parse error: {}", e.what());
                self->isSearching = false;
                self->SearchErrorBool = true;
                self->SearchErrorText = "Failed to parse search results: " + std::string(e.what());
            }
        });
    });
}

void WorkshopDownloader::FetchMapDetail(int index, int generation)
{
    if (stopRequested || searchGeneration != generation) {
        completedResults++;
        return;
    }

    std::string mapId;
    std::string mapName;

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        if (index >= (int)mapResults.size()) {
            completedResults++;
            return;
        }
        mapId = mapResults[index].ID;
        mapName = mapResults[index].Name;
    }

    std::string detailUrl = apiBase + "/" + mapId;
    LOG("Fetching detail for '{}' (ID: {})", mapName, mapId);

    CurlRequest req;
    req.url = detailUrl;

    std::weak_ptr<WorkshopDownloader> weak_self = shared_from_this();

    HttpWrapper::SendCurlRequest(req, [weak_self, index, generation, mapName, mapId](int code, std::string responseText) {
        auto self = weak_self.lock();
        if (!self) return;

        if (self->stopRequested || self->searchGeneration != generation) {
            self->completedResults++;
            return;
        }

        if (code == 200) {
            try {
                nlohmann::json j = nlohmann::json::parse(responseText);

                WorkshopRelease release;
                release.tag_name = SafeGetString(j, "latestVersionString", "v1.0");
                release.name = release.tag_name;

                if (j.contains("files") && j["files"].is_array() && !j["files"].empty()) {
                    auto& file = j["files"][0];
                    release.downloadLink = SafeGetString(file, "edgeUrl", "");
                    release.zipName = SafeGetString(file, "fileName", mapName + ".zip");
                }

                std::string fullDescription = SafeGetString(j, "description", "");

                {
                    std::lock_guard<std::mutex> lock(self->resultsMutex);

                    if (self->searchGeneration.load() != generation) {
                        self->completedResults++;
                        return;
                    }

                    if (index >= 0 && index < (int)self->mapResults.size() && self->mapResults[index].ID == mapId) {
                        auto& map = self->mapResults[index];

                        if (!fullDescription.empty()) {
                            map.Description = fullDescription;
                            self->CleanHTML(map.Description);
                        }

                        if (!release.downloadLink.empty()) {
                            map.releases.push_back(release);
                        }

                        // Start preview image download if we have a URL
                        if (!map.PreviewUrl.empty()) {
                            std::string imageExt = GetImageExtension(map.PreviewUrl);
                            map.ImageExtension = imageExt;
                            fs::path imagePath = self->BakkesmodPath + "SuiteSpot\\Workshop\\img\\" + mapId + imageExt;

                            if (self->DirectoryOrFileExists(imagePath)) {
                                map.ImagePath = imagePath;
                                map.isImageLoaded = true;
                            } else {
                                map.IsDownloadingPreview = true;
                                self->DownloadPreviewImage(map.PreviewUrl, imagePath.string(), index, generation);
                            }
                        }

                        self->listVersion++;
                        LOG("Detail loaded for '{}' (index {})", mapName, index);
                    }
                }

            } catch (const std::exception& e) {
                LOG("Failed to parse detail for '{}': {}", mapName, e.what());
            }
        } else {
            LOG("Failed to fetch detail for '{}' (HTTP {})", mapName, code);
        }

        self->completedResults++;
    });
}

void WorkshopDownloader::StopSearch()
{
    stopRequested = true;
    searchGeneration++;

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        mapResults.clear();
        mapsFound = 0;
        listVersion++;
    }

    resultsCV.notify_all();
    isSearching = false;
    LOG("Search stopped.");
}

void WorkshopDownloader::DownloadMap(std::string folderpath, WorkshopMap map, WorkshopRelease release)
{
    UserIsChoosingYESorNO = true;

    while (UserIsChoosingYESorNO) {
        Sleep(100);
    }

    if (!AcceptTheDownload) {
        return;
    }

    std::string workshopSafeMapName = SanitizeMapName(map.Name);

    std::string Workshop_Dl_Path;
    if (folderpath.back() == '/' || folderpath.back() == '\\') {
        Workshop_Dl_Path = folderpath + workshopSafeMapName;
    } else {
        Workshop_Dl_Path = folderpath + "/" + workshopSafeMapName;
    }

    try {
        fs::create_directory(Workshop_Dl_Path);
        LOG("Workshop directory created: {}", Workshop_Dl_Path);
    } catch (const std::exception& ex) {
        LOG("Failed to create directory: {}", ex.what());
        FolderErrorText = ex.what();
        FolderErrorBool = true;
        return;
    }

    CreateJSONLocalWorkshopInfos(workshopSafeMapName, Workshop_Dl_Path + "/", map.Name, map.Author, map.Description,
                                 map.PreviewUrl);
    LOG("JSON created: {}/{}.json", Workshop_Dl_Path, workshopSafeMapName);

    if (DirectoryOrFileExists(map.ImagePath)) {
        std::string imageExt = map.ImageExtension.empty() ? ".jfif" : map.ImageExtension;
        fs::copy(map.ImagePath, Workshop_Dl_Path + "/" + workshopSafeMapName + imageExt);
        LOG("Preview pasted: {}/{}{}", Workshop_Dl_Path, workshopSafeMapName, imageExt);
    }

    std::string download_url = release.downloadLink;
    LOG("Download URL: {}", download_url);
    std::string Folder_Path = Workshop_Dl_Path + "/" + release.zipName;

    downloadedBytes = 0;
    downloadProgress = 0;
    isDownloading = true;

    LOG("Download starting...");

    CurlRequest req;
    req.url = download_url;
    req.progress_function = [this](double file_size, double downloaded, ...) {
        downloadProgress = downloaded;
        downloadFileSize = file_size;
    };

    HttpWrapper::SendCurlRequest(req, [this, Folder_Path, Workshop_Dl_Path](int code, char* data, size_t size) {
        if (code == 200) {
            std::ofstream out_file{Folder_Path, std::ios_base::binary};
            if (out_file) {
                out_file.write(data, size);
                out_file.close();
                LOG("Workshop downloaded to: {}", Workshop_Dl_Path);
                isDownloading = false;
            }
        } else {
            LOG("Workshop download failed with code {}", code);
            isDownloading = false;
        }
    });

    while (isDownloading) {
        downloadedBytes = downloadProgress.load();
        Sleep(500);
    }

    // Extract silently without stealing focus
    LOG("Extracting zip: {}", Folder_Path);
    Utils::ExpandArchive(Folder_Path, Workshop_Dl_Path);

    int checkTime = 0;
    while (UdkInDirectory(Workshop_Dl_Path) == "Null") {
        if (checkTime > 10) {
            LOG("Failed extracting the map zip file");
            return;
        }
        Sleep(1000);
        checkTime++;
    }

    LOG("File Extracted");
    RenameFileToUPK(Workshop_Dl_Path);
}

void WorkshopDownloader::DownloadPreviewImage(std::string downloadUrl, std::string filePath, int mapResultIndex,
                                              int generation)
{
    if (downloadUrl.empty()) {
        return;
    }

    fs::create_directories(fs::path(filePath).parent_path());

    CurlRequest req;
    req.url = downloadUrl;

    std::weak_ptr<WorkshopDownloader> weak_self = shared_from_this();

    HttpWrapper::SendCurlRequest(req, [weak_self, filePath, mapResultIndex, generation](int code, char* data, size_t size) {
        auto self = weak_self.lock();
        if (!self) return;

        // Check if this callback is still valid for the current search
        if (self->searchGeneration.load() != generation) {
            return;
        }

        if (code == 200) {
            try {
                std::ofstream outFile(filePath, std::ios::binary);
                if (outFile) {
                    outFile.write(data, size);
                    outFile.close();

                    {
                        std::lock_guard<std::mutex> lock(self->resultsMutex);
                        // Double check bounds and generation inside lock
                        if (self->searchGeneration.load() == generation && mapResultIndex >= 0 &&
                            mapResultIndex < (int)self->mapResults.size()) {
                            self->mapResults[mapResultIndex].ImagePath = filePath;
                            self->mapResults[mapResultIndex].IsDownloadingPreview = false;
                            self->listVersion++;
                        }
                    }

                    LOG("Preview downloaded: {}", filePath);
                }
            } catch (const std::exception& e) {
                LOG("Error writing preview file {}: {}", filePath, e.what());
            }
        }
    });
}

void WorkshopDownloader::CreateJSONLocalWorkshopInfos(std::string jsonFileName, std::string workshopMapPath,
                                                      std::string mapTitle, std::string mapAuthor,
                                                      std::string mapDescription, std::string mapPreviewUrl)
{
    std::string filename = workshopMapPath + jsonFileName + ".json";
    std::ofstream JSONFile(filename);

    nlohmann::json j;
    j["Title"] = mapTitle;
    j["Author"] = mapAuthor;
    j["Description"] = mapDescription;
    j["PreviewUrl"] = mapPreviewUrl;

    JSONFile << j.dump();
    JSONFile.close();
}

int WorkshopDownloader::ExtractZipPowerShell(std::string zipFilePath, std::string destinationPath)
{
    return Utils::ExpandArchive(zipFilePath, destinationPath);
}

void WorkshopDownloader::RenameFileToUPK(fs::path filePath)
{
    std::string udkFile = UdkInDirectory(filePath.string());

    if (udkFile != "Null") {
        fs::path udkPath = filePath / udkFile;
        std::string upkName = udkFile.substr(0, udkFile.length() - 4) + ".upk";
        fs::path upkPath = filePath / upkName;

        try {
            fs::rename(udkPath, upkPath);
            LOG("Renamed {} to {}", udkFile, upkName);
        } catch (const std::exception& e) {
            LOG("Failed to rename .udk to .upk: {}", e.what());
        }
    }
}

std::string WorkshopDownloader::UdkInDirectory(std::string dirPath)
{
    if (!DirectoryOrFileExists(fs::path(dirPath))) {
        return "Null";
    }

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".udk") {
                return entry.path().filename().string();
            }
        }
    }

    return "Null";
}

std::string WorkshopDownloader::SanitizeMapName(const std::string& name)
{
    std::string safe = name;
    ReplaceAll(safe, " ", "_");

    std::string specials[] = {"/", "\\", "?", ":", "*", "\"", "<", ">", "|", "-", "#"};
    for (auto special : specials) {
        EraseAll(safe, special);
    }

    return safe;
}

void WorkshopDownloader::CleanHTML(std::string& S)
{
    size_t pos = 0;
    while ((pos = S.find('<')) != std::string::npos) {
        size_t endPos = S.find('>', pos);
        if (endPos != std::string::npos) {
            S.erase(pos, endPos - pos + 1);
        } else {
            break;
        }
    }
}

void WorkshopDownloader::EraseAll(std::string& str, const std::string& from)
{
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.erase(pos, from.length());
    }
}

void WorkshopDownloader::ReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

bool WorkshopDownloader::DirectoryOrFileExists(const fs::path& p)
{
    return fs::exists(p);
}
```

## File: src/core/WorkshopDownloader.h
```c
#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/wrappers/http/HttpWrapper.h"
#include "MapList.h"
#include "logging.h"
#include "IMGUI/json.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace fs = std::filesystem;

struct WorkshopRelease
{
    std::string name;
    std::string tag_name;
    std::string description;
    std::string zipName;
    std::string downloadLink;
    std::string pictureLink;
};

struct WorkshopMap
{
    std::string ID;
    std::string Name;
    std::string Size;
    std::string Description;
    std::string PreviewUrl;
    std::string Author;
    std::vector<WorkshopRelease> releases;
    fs::path ImagePath;
    std::string ImageExtension;
    std::shared_ptr<ImageWrapper> Image;
    bool isImageLoaded = false;
    bool IsDownloadingPreview = false;
};

class WorkshopDownloader : public std::enable_shared_from_this<WorkshopDownloader>
{
  public:
    WorkshopDownloader(std::shared_ptr<GameWrapper> gw);
    ~WorkshopDownloader();

    void GetResults(std::string keyWord);
    void FetchMapDetail(int index, int generation);

    void DownloadMap(std::string folderpath, WorkshopMap map, WorkshopRelease release);
    void DownloadPreviewImage(std::string downloadUrl, std::string filePath, int mapIndex, int generation);

    void CreateJSONLocalWorkshopInfos(std::string jsonFileName, std::string workshopMapPath, std::string mapTitle,
                                      std::string mapAuthor, std::string mapDescription, std::string mapPreviewUrl);
    int ExtractZipPowerShell(std::string zipFilePath, std::string destinationPath);
    void RenameFileToUPK(fs::path filePath);
    std::string UdkInDirectory(std::string dirPath);

    void StopSearch();

    std::atomic<bool> isSearching = false;
    std::atomic<int> mapsFound = 0;
    std::vector<WorkshopMap> mapResults;

    std::atomic<bool> isDownloading = false;
    std::atomic<int> downloadProgress = 0;
    std::atomic<int> downloadedBytes = 0;
    std::atomic<int> downloadFileSize = 0;

    // Download confirmation flags (thread spin-waits for UI)
    std::atomic<bool> UserIsChoosingYESorNO = false;
    std::atomic<bool> AcceptTheDownload = false;

    std::atomic<bool> FolderErrorBool = false;
    std::string FolderErrorText;

    std::atomic<bool> SearchErrorBool = false;
    std::string SearchErrorText;

    std::string BakkesmodPath;
    std::string IfNoPreviewImagePath;
    std::string apiBase = "https://bakkesplugins.com/api/rocket-league-maps";

    mutable std::mutex resultsMutex;
    std::condition_variable resultsCV;
    std::atomic<int> completedRequests = 0;
    std::atomic<int> completedResults = 0;
    std::atomic<int> expectedResults = 0;
    std::atomic<int> searchGeneration = 0;
    std::atomic<bool> stopRequested = false;
    std::atomic<int> listVersion = 0;

    int GetSearchGeneration() const { return searchGeneration.load(); }

    std::string SanitizeMapName(const std::string& name);

  private:
    std::shared_ptr<GameWrapper> gameWrapper;
    std::thread searchThread;

    void CleanHTML(std::string& S);
    void EraseAll(std::string& str, const std::string& from);
    void ReplaceAll(std::string& str, const std::string& from, const std::string& to);
    bool DirectoryOrFileExists(const fs::path& p);
};
```

## File: src/SuiteSpot.cpp
```cpp
#include "pch.h"
#include "SuiteSpot.h"
#include "MapList.h"
#include "MapManager.h"
#include "SettingsSync.h"
#include "AutoLoadFeature.h"
#include "TrainingPackManager.h"
#include "WorkshopDownloader.h"
#include "SettingsUI.h"
#include "TrainingPackUI.h"
#include "LoadoutUI.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"
#include "bakkesmod/wrappers/GameEvent/TrainingEditorWrapper.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <random>
#include <iomanip>
#include <sstream>
#include <cmath>

/*
 * ======================================================================================
 * SUITESPOT: IMPLEMENTATION DETAILS
 * ======================================================================================
 * 
 * This file contains the actual code for the plugin's lifecycle.
 * 
 * KEY SECTIONS:
 * 1. PERSISTENCE HELPERS: Tiny functions that ask the Managers for data paths (like where 
 *    to find Workshop maps).
 * 2. EVENT HOOKS: The `GameEndedEvent` function is the heartbeat of the automation. 
 *    It waits for the match to finish, then triggers the `AutoLoadFeature`.
 * 3. LOADING (onLoad):
 *    - Creates all the "Managers" (tools for Maps, Settings, Packs).
 *    - Registers the "Browser Window" logic (via togglemenu).
 *    - Hooks into the game events.
 * 4. RENDERING:
 *    - Handled natively by BakkesMod's PluginWindow interface.
 *    - Uses a custom OnClose override to ensure standalone windows stay persistent
 *      when the main settings menu is closed.
 */

// ===== SuiteSpot persistence helpers =====
std::filesystem::path SuiteSpot::GetDataRoot() const
{
    return mapManager ? mapManager->GetDataRoot() : std::filesystem::path();
}

std::filesystem::path SuiteSpot::GetSuiteTrainingDir() const
{
    return mapManager ? mapManager->GetSuiteTrainingDir() : std::filesystem::path();
}

void SuiteSpot::EnsureDataDirectories() const
{
    if (mapManager) {
        mapManager->EnsureDataDirectories();
    }
}

std::filesystem::path SuiteSpot::GetWorkshopLoaderConfigPath() const
{
    return mapManager ? mapManager->GetWorkshopLoaderConfigPath() : std::filesystem::path();
}

std::filesystem::path SuiteSpot::ResolveConfiguredWorkshopRoot() const
{
    return mapManager ? mapManager->ResolveConfiguredWorkshopRoot() : std::filesystem::path();
}

void SuiteSpot::DiscoverWorkshopInDir(const std::filesystem::path& dir)
{
    if (mapManager) {
        mapManager->DiscoverWorkshopInDir(dir, RLWorkshop);
    }
}

void SuiteSpot::LoadWorkshopMaps()
{
    if (mapManager) {
        // Load workshop maps without passing an index - the path-based selection persists automatically
        int unused = 0;
        mapManager->LoadWorkshopMaps(RLWorkshop, unused);
    }
}

// ===== TRAINING PACK UPDATE INTEGRATION =====
bool SuiteSpot::IsEnabled() const
{
    return settingsSync ? settingsSync->IsEnabled() : false;
}

bool SuiteSpot::IsAutoQueueEnabled() const
{
    return settingsSync ? settingsSync->IsAutoQueue() : false;
}

bool SuiteSpot::IsTrainingGameSpeedFixEnabled() const
{
    return settingsSync ? settingsSync->IsTrainingGameSpeedFixEnabled() : true;
}

int SuiteSpot::GetMapType() const
{
    return settingsSync ? settingsSync->GetMapType() : 0;
}

int SuiteSpot::GetDelayQueueSec() const
{
    return settingsSync ? settingsSync->GetDelayQueueSec() : 0;
}

int SuiteSpot::GetDelayFreeplaySec() const
{
    return settingsSync ? settingsSync->GetDelayFreeplaySec() : 0;
}

int SuiteSpot::GetDelayTrainingSec() const
{
    return settingsSync ? settingsSync->GetDelayTrainingSec() : 0;
}

int SuiteSpot::GetDelayWorkshopSec() const
{
    return settingsSync ? settingsSync->GetDelayWorkshopSec() : 0;
}

std::string SuiteSpot::GetCurrentFreeplayCode() const
{
    return settingsSync ? settingsSync->GetCurrentFreeplayCode() : "";
}

std::string SuiteSpot::GetCurrentTrainingCode() const
{
    return settingsSync ? settingsSync->GetCurrentTrainingCode() : "";
}

std::string SuiteSpot::GetCurrentWorkshopPath() const
{
    return settingsSync ? settingsSync->GetCurrentWorkshopPath() : "";
}

// #detailed comments: UpdateTrainingPackList
// Purpose: Launches an external PowerShell script to download the latest
// training pack data and write a JSON cache to disk. This is intentionally
// performed in a background task to avoid any blocking on the UI/game thread.
//
// Safety and behavior notes:
//  - scrapingInProgress is a guard flag ensuring only one update
//    runs at a time. It is set before launching and cleared when the
//    background process finishes.
//  - The implementation uses system() and relies on the platform's
//    default process creation semantics; this must remain as-is for
//    portability with existing deployments. If this is changed to a
//    more advanced process API, ensure identical detach/exit semantics.
//  - The script path is hard-coded to the repo dev path; callers should
//    ensure that the script is present when invoking this routine.
//
// DO NOT CHANGE: Modifying the background thread logic or the way the
// result is checked could resurface race conditions that previously
// required this exact coordination.
void SuiteSpot::UpdateTrainingPackList()
{
    if (trainingPackMgr) {
        trainingPackMgr->UpdateTrainingPackList(GetTrainingPacksPath(), gameWrapper);
    }
}

BAKKESMOD_PLUGIN(SuiteSpot, "SuiteSpot", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

float SuiteSpot::ConvertMenuPercentToDecimal(float menuValue)
{
    if (!std::isfinite(menuValue)) {
        return 1.0f;
    }

    return std::clamp(menuValue, 0.01f, 10.0f);
}

void SuiteSpot::ApplyTrainingGameSpeedFromMenuValue(float menuValue)
{
    officialTrainingGameSpeed = ConvertMenuPercentToDecimal(menuValue);

    if (!IsTrainingGameSpeedFixEnabled()) {
        return;
    }

    CVarWrapper speedCvar = cvarManager->getCvar("sv_soccar_gamespeed");
    if (!speedCvar) {
        return;
    }

    speedCvar.setValue(officialTrainingGameSpeed);
    LOG("Training game speed set to {}", officialTrainingGameSpeed);
}

void SuiteSpot::LoadTrainingGameSpeedHooks()
{
    gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GFxData_Settings_TA.SetTrainingGameSpeed",
                                                   [this](ActorWrapper caller, void* params, std::string eventName) {
                                                       if (params == nullptr) {
                                                           return;
                                                       }

                                                       // SetTrainingGameSpeed params have 8 bytes of padding before the float
                                                       struct SetTrainingGameSpeedParams
                                                       {
                                                           unsigned char _pad[0x8];
                                                           float value;
                                                       };
                                                       float rawValue =
                                                           reinterpret_cast<SetTrainingGameSpeedParams*>(params)->value;

                                                       ApplyTrainingGameSpeedFromMenuValue(rawValue);
                                                   });
}

void SuiteSpot::UnloadTrainingGameSpeedHooks()
{
    gameWrapper->UnhookEvent("Function TAGame.GFxData_Settings_TA.SetTrainingGameSpeed");
}

void SuiteSpot::LoadHooks()
{
    // ===== MATCH EVENT HOOKS =====
    // Re-queue/transition at match end. We use HookEventPost to ensure the game has finished
    // its internal match-end logic before we attempt to load a new map.
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
                               [this](std::string eventName) { GameEndedEvent(eventName); });

    // ===== PACK HEALER - Training Events =====
    // Based on BakkesMod SDK reference documentation

    // Hook: Training pack loaded or restarted
    // Note: IsInCustomTraining() will not yet return true at this point
    gameWrapper->HookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnInit", [this](std::string eventName) {
        if (!IsEnabled()) return;
        LOG("Hook triggered: GameEvent_TrainingEditor_TA.OnInit");
        gameWrapper->SetTimeout([this](GameWrapper* gw) { TryHealCurrentPack(gw); }, 1.5f);
    });

    // Hook: Shot attempt started (player moves)
    // This fires when switching shots and player starts moving
    gameWrapper->HookEventPost("Function TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt",
                               [this](std::string eventName) {
                                   LOG("Hook triggered: TrainingEditorMetrics_TA.TrainingShotAttempt");
                                   // Note: This hook exists but is not currently used for auto-heal
                               });

    // Manual heal command
    cvarManager->registerNotifier(
        "ss_heal_current_pack", [this](std::vector<std::string> args) { TryHealCurrentPack(gameWrapper.get()); },
        "Manually heal the currently loaded training pack", PERMISSION_ALL);

    // Hotkey actions handled entirely in HandleKeyPress hook — no setBind/notifiers.
    // Dual-key strictly required: key1 = trigger, key2 = must be held. Both must be non-empty.
    gameWrapper->HookEventWithCaller<
        ActorWrapper>("Function TAGame.GameViewportClient_TA.HandleKeyPress", [this](ActorWrapper caller, void* params,
                                                                                     std::string eventName) {
        auto p = static_cast<HandleKeyPressParams*>(params);
        std::string keyName = gameWrapper->GetFNameByIndex(p->KeyIndex);
        if (keyName.empty() || keyName == "None") return;

        // Maintain heldKeys set (press = insert, release = erase).
        if (p->EventType == 0) // IE_Pressed
            heldKeys.insert(keyName);
        else if (p->EventType == 1) // IE_Released
            heldKeys.erase(keyName);

        // Fire hotkey actions on press only. Both keys must be non-empty (dual-key enforced).
        if (p->EventType == 0 && settingsSync && mapManager) {
            // Returns true if key1 matches the pressed key and key2 is currently held.
            auto check = [&](const std::string& k1, const std::string& k2) -> bool {
                return !k1.empty() && !k2.empty() && keyName == k1 && heldKeys.count(k2) > 0;
            };

            if (check(settingsSync->GetHotkeyMapModeFwdKey1(), settingsSync->GetHotkeyMapModeFwdKey2())) {
                LOG("Hotkey: cycle_map_mode_fwd");
                mapManager->CycleMapMode(true);
                int newMode = mapManager->GetCurrentMapModeIndex();
                static constexpr const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
                gameWrapper->Toast("", std::string("Map Mode: ") + modeNames[newMode], "default", 3.5f, ToastType_Info);
                cvarManager->getCvar("suitespot_map_type").setValue(newMode);
            } else if (check(settingsSync->GetHotkeyMapModeBkKey1(), settingsSync->GetHotkeyMapModeBkKey2())) {
                LOG("Hotkey: cycle_map_mode_bk");
                mapManager->CycleMapMode(false);
                int newMode = mapManager->GetCurrentMapModeIndex();
                static constexpr const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
                gameWrapper->Toast("", std::string("Map Mode: ") + modeNames[newMode], "default", 3.5f, ToastType_Info);
                cvarManager->getCvar("suitespot_map_type").setValue(newMode);
            } else if (check(settingsSync->GetHotkeyCycleMapFwdKey1(), settingsSync->GetHotkeyCycleMapFwdKey2())) {
                LOG("Hotkey: cycle_map_fwd");
                mapManager->CycleMap(true);
                int mode = mapManager->GetCurrentMapModeIndex();
                std::string mapName;
                if (mode == 0) {
                    mapName = mapManager->GetCurrentFreeplayName();
                    cvarManager->getCvar("suitespot_current_freeplay_code").setValue(mapManager->GetCurrentFreeplayCode());
                } else if (mode == 1) {
                    mapName = mapManager->GetCurrentTrainingName();
                    cvarManager->getCvar("suitespot_current_training_code").setValue(mapManager->GetCurrentTrainingCode());
                } else if (mode == 2) {
                    mapName = mapManager->GetCurrentWorkshopName();
                    cvarManager->getCvar("suitespot_current_workshop_path").setValue(mapManager->GetCurrentWorkshopPath());
                }
                gameWrapper->Toast("", "Map: " + mapName, "default", 3.5f, ToastType_Info);
            } else if (check(settingsSync->GetHotkeyCycleMapBkKey1(), settingsSync->GetHotkeyCycleMapBkKey2())) {
                LOG("Hotkey: cycle_map_bk");
                mapManager->CycleMap(false);
                int mode = mapManager->GetCurrentMapModeIndex();
                std::string mapName;
                if (mode == 0) {
                    mapName = mapManager->GetCurrentFreeplayName();
                    cvarManager->getCvar("suitespot_current_freeplay_code").setValue(mapManager->GetCurrentFreeplayCode());
                } else if (mode == 1) {
                    mapName = mapManager->GetCurrentTrainingName();
                    cvarManager->getCvar("suitespot_current_training_code").setValue(mapManager->GetCurrentTrainingCode());
                } else if (mode == 2) {
                    mapName = mapManager->GetCurrentWorkshopName();
                    cvarManager->getCvar("suitespot_current_workshop_path").setValue(mapManager->GetCurrentWorkshopPath());
                }
                gameWrapper->Toast("", "Map: " + mapName, "default", 3.5f, ToastType_Info);
            } else if (check(settingsSync->GetHotkeyLoadNowKey1(), settingsSync->GetHotkeyLoadNowKey2())) {
                LOG("Hotkey: load_now");
                int mapType = settingsSync->GetMapType();
                std::string cmd;
                if (mapType == 0) {
                    auto code = settingsSync->GetCurrentFreeplayCode();
                    if (!code.empty()) cmd = "load_freeplay " + code;
                } else if (mapType == 1) {
                    auto code = settingsSync->GetCurrentTrainingCode();
                    if (!code.empty()) cmd = "load_training " + code;
                } else if (mapType == 2) {
                    auto path = settingsSync->GetCurrentWorkshopPath();
                    if (!path.empty()) cmd = "load_workshop \"" + path + "\"";
                }
                if (!cmd.empty()) {
                    gameWrapper->Execute([this, cmd](GameWrapper*) { cvarManager->executeCommand(cmd); });
                } else {
                    gameWrapper->Toast("", "No map selected", "default", 3.5f, ToastType_Error);
                }
            }
        }

        // Hotkey capture UI — only active when captureRow >= 0.
        if (captureRow < 0) return;
        if (p->EventType != 0) return; // capture on press only

        LOG("Hotkey Capture: Detected key {} (Index: {}, Gamepad: {})", keyName, p->KeyIndex, p->bGamepad);

        if (keyName == "Escape") {
            LOG("Hotkey Capture: Cancelled via Escape");
            captureRow = -1;
            return;
        }

        if (captureRow < 5) {
            const auto& row = UI::SettingsUI::HOTKEY_ROWS[captureRow];
            const char* cvarName = (captureSlot == 0) ? row.key1CVar : row.key2CVar;
            LOG("Hotkey Capture: Assigning {} to {}", keyName, cvarName);
            gameWrapper->Execute([this, cvarName, keyName](GameWrapper* gw) {
                UI::Helpers::SetCVarSafely(cvarName, keyName, cvarManager, gameWrapper);
            });
            captureRow = -1;
        }
    });
}

// #detailed comments: GameEndedEvent
// Purpose: Called by hooked game events when a match ends. The function
// runs the auto-load logic if enabled.
//
// Timing and ordering notes:
//  - The postMatch.start timestamp is recorded with steady_clock so
//    overlay lifetime calculations are not affected by system clock
//    adjustments.
//
// DO NOT CHANGE: The safeExecute lambda intentionally accepts a delay
// (in seconds) and either executes immediately or schedules via
// gameWrapper->SetTimeout. Changing its semantics will alter when
// external commands (load_freeplay, queue, etc.) are run relative to
// overlay presentation.
void SuiteSpot::GameEndedEvent(std::string name)
{
    if (!IsEnabled()) return;

    LOG("SuiteSpot: GameEndedEvent triggered by hook: {}", name);

    // 1. Run Auto-Load/Queue Logic first (Independent of overlay)
    if (autoLoadFeature && settingsSync) {
        LOG("SuiteSpot: Triggering AutoLoadFeature::OnMatchEnded");

        autoLoadFeature->OnMatchEnded(gameWrapper, cvarManager, RLMaps, RLTraining, RLWorkshop, *settingsSync,
                                      usageTracker.get());

        // Usage tracking handled by AutoLoadFeature::OnMatchEnded
    }
}

// Helper method to extract and heal pack data from current training session
void SuiteSpot::TryHealCurrentPack(GameWrapper* gw)
{
    if (!trainingPackMgr) {
        LOG("SuiteSpot: TryHealCurrentPack - trainingPackMgr is null");
        return;
    }

    if (!gw) {
        LOG("SuiteSpot: TryHealCurrentPack - GameWrapper is null");
        return;
    }

    if (!gw->IsInCustomTraining()) {
        LOG("SuiteSpot: TryHealCurrentPack - Not in custom training (IsInCustomTraining=false)");
        return;
    }

    LOG("SuiteSpot: TryHealCurrentPack - In custom training, attempting to get data...");

    auto server = gw->GetGameEventAsServer();
    if (!server) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to get GameEventAsServer");
        return;
    }

    TrainingEditorWrapper editor(server.memory_address);
    if (!editor) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to create TrainingEditorWrapper");
        return;
    }

    auto trainingData = editor.GetTrainingData();
    if (!trainingData) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to get TrainingData");
        return;
    }

    auto saveData = trainingData.GetTrainingData();
    if (!saveData) {
        LOG("SuiteSpot: TryHealCurrentPack - Failed to get TrainingEditorSaveData");
        return;
    }

    std::string code = saveData.GetCode().ToString();

    if (code.empty()) {
        LOG("SuiteSpot: TryHealCurrentPack - Pack code is empty");
        return;
    }

    // Try multiple methods to get shot count
    int realShots = 0;

    // Method 1: From TrainingEditorWrapper directly
    realShots = editor.GetTotalRounds();
    LOG("SuiteSpot: Method 1 (editor.GetTotalRounds): {}", realShots);

    // Method 2: From save data (backup)
    if (realShots <= 0) {
        realShots = saveData.GetNumRounds();
        LOG("SuiteSpot: Method 2 (saveData.GetNumRounds): {}", realShots);
    }

    if (realShots <= 0) {
        LOG("SuiteSpot: [ERR] All methods failed to extract shot count (got {})", realShots);
        return;
    }

    LOG("SuiteSpot: [OK] Successfully extracted pack data - Code: {}, Shots: {}", code, realShots);
    LOG("SuiteSpot: Calling HealPack...");
    trainingPackMgr->HealPack(code, realShots);
}

void SuiteSpot::onLoad()
{
    _globalCvarManager = cvarManager;
    LOG("SuiteSpot loaded");
    mapManager = std::make_unique<MapManager>();
    settingsSync = std::make_unique<SettingsSync>();
    autoLoadFeature = std::make_unique<AutoLoadFeature>();
    trainingPackMgr = std::make_unique<TrainingPackManager>();
    settingsUI = std::make_unique<SettingsUI>(this);
    trainingPackUI = std::make_shared<TrainingPackUI>(this);
    loadoutUI = std::make_unique<LoadoutUI>(this);

    EnsureDataDirectories();
    LoadWorkshopMaps();

    // Initialize LoadoutManager
    loadoutManager = std::make_unique<LoadoutManager>(gameWrapper);
    LOG("SuiteSpot: LoadoutManager initialized");

    // Initialize PackUsageTracker
    usageTracker = std::make_unique<PackUsageTracker>(GetSuiteTrainingDir() / "pack_usage_stats.json");
    LOG("SuiteSpot: PackUsageTracker initialized");

    // Initialize WorkshopDownloader
    workshopDownloader = std::make_shared<WorkshopDownloader>(gameWrapper);
    LOG("SuiteSpot: WorkshopDownloader initialized");

    // Initialize TextureDownloader
    textureDownloader = std::make_unique<TextureDownloader>(gameWrapper, cvarManager);
    LOG("SuiteSpot: TextureDownloader initialized");

    // Check Pack cache and load if available

    if (trainingPackMgr) {
        if (!std::filesystem::exists(GetTrainingPacksPath())) {
            LOG("SuiteSpot: No Pack cache found. Schedule scraping on next opportunity.");
            // Will be scraped on first Settings render or user request
        } else {
            // Load existing Pack cache
            trainingPackMgr->LoadPacksFromFile(GetTrainingPacksPath());
            LOG("SuiteSpot: Pack cache loaded");
        }
    }

    LoadHooks();

    if (settingsSync) {
        settingsSync->RegisterAllCVars(cvarManager);
        // Restore saved CVar values (hotkeys, etc.) that may have been reset by re-registration on hot-reload.
        cvarManager->loadCfg("config");

        // Always check workshop textures on launch — required for workshop feature to work
        if (textureDownloader) {
            LOG("SuiteSpot: Checking workshop textures...");
            std::vector<std::string> missing = textureDownloader->CheckMissingTextures();
            if (missing.empty()) {
                LOG("SuiteSpot: All {} workshop textures present.", textureDownloader->WorkshopTexturesFilesList.size());
            } else {
                LOG("SuiteSpot: {} missing texture(s) detected. Auto-downloading...", missing.size());
                if (textureDownloadThread.joinable()) {
                    textureDownloadThread.join();
                }
                textureDownloadThread = std::thread([this]() { textureDownloader->DownloadAndInstallTextures(); });
            }
        }
    }
    LoadTrainingGameSpeedHooks();

    LOG("SuiteSpot: Plugin initialization complete");
}

// #detailed comments: onUnload
// Purpose: Clean up all SDK resources in proper order to enable hot-reload.
//
// Cleanup sequence (CRITICAL - must follow this order):
//  1. Join all background threads (prevents dangling thread callbacks)
//  2. Save pending data (ensures no data loss)
//  3. Unhook events (removes game event callbacks pointing to freed memory)
//  4. Reset UI components (releases ImGui resources)
//  5. Reset managers (releases business logic)
//  6. Clear ImGui context
//
// Note: BakkesMod automatically cleans up CVars and notifiers on plugin unload,
// but event hooks MUST be manually unhooked to prevent hot-reload crashes.
//
// DO NOT CHANGE: This sequence prevents hot-reload crashes by ensuring
// all callbacks are removed before the plugin DLL is unloaded. Skipping
// event unhooking will cause BakkesMod to call freed memory on reload.
void SuiteSpot::onUnload()
{
    LOG("SuiteSpot unloading...");

    // Wait for texture download to complete if running
    if (textureDownloadThread.joinable()) {
        LOG("SuiteSpot: Waiting for texture download to complete...");
        textureDownloadThread.join();
    }

    // Stop workshop downloader search thread
    if (workshopDownloader) {
        workshopDownloader->StopSearch();
    }

    if (usageTracker) {
        usageTracker->SaveStats();
    }

    UnloadTrainingGameSpeedHooks();

    // STEP 3: Unhook all game events (CRITICAL - SDK requirement)
    gameWrapper->UnhookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded");
    gameWrapper->UnhookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnInit");
    gameWrapper->UnhookEventPost("Function TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt");
    gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleKeyPress");
    LOG("Event hooks removed");

    // STEP 4: Reset UI components (releases ImGui resources)
    settingsUI.reset();
    trainingPackUI.reset();
    loadoutUI.reset();
    LOG("UI components destroyed");

    // STEP 5: Reset data managers
    trainingPackMgr.reset();
    autoLoadFeature.reset();
    settingsSync.reset();
    mapManager.reset();
    workshopDownloader.reset();
    LOG("Managers destroyed");

    // STEP 6: Clear ImGui context
    imgui_ctx = 0;

    LOG("SuiteSpot unloaded successfully");
}

void SuiteSpot::Render()
{
    if (!imgui_ctx) return;
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(imgui_ctx));

    // Note: TrainingPackUI is a PluginWindow registered with BakkesMod,
    // so it's rendered automatically by the framework. No need to call it here.
}

std::string SuiteSpot::GetMenuName()
{

    return "suitespot_browser";
}

std::string SuiteSpot::GetMenuTitle()
{

    return "SuiteSpot Training Browser";
}

void SuiteSpot::SetImGuiContext(uintptr_t ctx)
{

    if (ctx) {

        imgui_ctx = ctx;

        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));

        // Load clock font — GetFont first (hot-reload: font already in atlas, no rebuild)
        // Falls back to Execute + LoadFont (cold start: defers atlas rebuild to game thread)
        // Guard inside lambda: Execute may be queued multiple times if SetImGuiContext fires
        // before the first callback runs; the inner check prevents duplicate atlas rebuilds.
        if (!clockFont) {
            auto gui = gameWrapper->GetGUIManager();
            clockFont = gui.GetFont("suitespot_clock_48");
            if (!clockFont) {
                gameWrapper->Execute([this](GameWrapper* gw) {
                    if (clockFont) return; // already loaded by an earlier Execute
                    auto gui = gw->GetGUIManager();
                    auto [res, font] = gui.LoadFont("suitespot_clock_48", "Ubuntu-Regular.ttf", 48);
                    if (res == 2 && font) {
                        clockFont = font;
                    }
                });
            }
        }

        // Load UI font: Roboto-Medium 14px + FA5 Solid icons merged in (glyph range F000-F8D9)
        if (!uiFont) {
            auto gui = gameWrapper->GetGUIManager();
            uiFont = gui.GetFont("suitespot_roboto_14");
            if (!uiFont) {
                gameWrapper->Execute([this](GameWrapper* gw) {
                    if (uiFont) return; // already loaded by an earlier Execute
                    auto gui = gw->GetGUIManager();
                    // Base font: Roboto Medium at 14px
                    auto [r1, roboto] = gui.LoadFont("suitespot_roboto_14", "Roboto-Medium.ttf", 14);
                    if (r1 == 2 && roboto) uiFont = roboto;
                    // Merge FA5 Solid icons into Roboto (must immediately follow base font load)
                    static ImFontConfig fa_cfg;
                    fa_cfg.MergeMode = true;
                    fa_cfg.PixelSnapH = true;
                    fa_cfg.GlyphMinAdvanceX = 13.0f;
                    static const ImWchar fa_ranges[] = {0xf000, 0xf8d9, 0}; // FA5 Solid range
                    gui.LoadFont("suitespot_fa_solid_14", "fa-solid-900.ttf", 14, &fa_cfg, fa_ranges);
                });
            }
        }
    }
}

bool SuiteSpot::ShouldBlockInput()
{
    if (!isBrowserOpen) {
        return false; // Browser closed → no blocking
    }

    // Selective input blocking - consistent with TrainingPackUI
    ImGuiIO& io = ImGui::GetIO();

    // Block when actively typing in text fields (settings UI)
    if (io.WantTextInput && ImGui::IsAnyItemActive()) {
        return true;
    }

    // Allow normal mouse interaction without blocking game input
    return false;
}

bool SuiteSpot::IsActiveOverlay()
{

    return isBrowserOpen;
}

void SuiteSpot::OnOpen()
{

    LOG("SuiteSpot: OnOpen called");

    isBrowserOpen = true;

    if (trainingPackUI) {

        trainingPackUI->SetOpen(true);
    }
}

void SuiteSpot::OnClose()
{
    LOG("SuiteSpot: OnClose called (Ignoring state change to keep browser open)");
}

std::filesystem::path SuiteSpot::GetTrainingPacksPath() const
{
    return mapManager ? mapManager->GetTrainingPacksPath() : std::filesystem::path();
}

void SuiteSpot::LoadTrainingPacksFromFile(const std::filesystem::path& filePath)
{
    if (trainingPackMgr) {
        trainingPackMgr->LoadPacksFromFile(filePath);
    }
}
```

## File: src/SuiteSpot.h
```c
#pragma once

/*
 * ======================================================================================
 * SUITESPOT: PLUGIN ARCHITECTURE & MAIN HUB
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This is the "Brain" of the SuiteSpot plugin. The `SuiteSpot` class is the central
 * hub that connects BakkesMod (the game) to all our custom features.
 * 
 * WHY IS IT HERE?
 * BakkesMod requires one main class that inherits from `BakkesModPlugin` to start
 * everything up. This file tells the game "I am a plugin, here is how to load me."
 * 
 * HOW DOES IT WORK?
 * 1. LIFECYCLE:
 *    - `onLoad()`: Called when Rocket League starts. We create all our tools (Managers, UI) here.
 *    - `onUnload()`: Called when the game closes. We clean up memory here.
 * 
 * 2. THE BRIDGE:
 *    - It holds pointers to "Managers" (MapManager, TrainingPackManager) which handle data.
 *    - It holds pointers to "UI" (SettingsUI, TrainingPackUI) which draw the menus.
 *    - It listens for game events (like "Match Ended") and tells the `AutoLoadFeature` to react.
 * 
 * 3. WINDOW MANAGEMENT (Advanced):
 *    - This class is special because it acts as BOTH the "Settings Tab" (inside F2 menu)
 *      AND the "Window Manager" for our pop-up browser.
 *    - It uses a "Hybrid Rendering" trick to keep the browser window open even when F2 is closed.
 *      (See `docs/architecture.md` for the technical details).
 */

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "MapList.h"
#include "LoadoutManager.h"
#include "PackUsageTracker.h"
#include "TextureDownloader.h"
#include "version.h"
#include <filesystem>
#include <set>
#include <memory>

#include <atomic>

class SettingsWindowBase : public BakkesMod::Plugin::PluginSettingsWindow
{
  public:
    virtual ~SettingsWindowBase() = default;
    std::string GetPluginName() override { return "SuiteSpot"; }
    void SetImGuiContext(uintptr_t ctx) override { ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx)); }
};

// Forward declarations
class MapManager;
class SettingsSync;
class AutoLoadFeature;
class TrainingPackManager;
class WorkshopDownloader;
class SettingsUI;
class TrainingPackUI;
class LoadoutUI;

// Version macro carried over from the master template
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(
    VERSION_PATCH) "." stringify(VERSION_BUILD);

class SuiteSpot final : public BakkesMod::Plugin::BakkesModPlugin,
                        public SettingsWindowBase,
                        public BakkesMod::Plugin::PluginWindow
{
    friend class SettingsUI;
    friend class TrainingPackUI;
    friend class LoadoutUI;

  public:
    // PluginWindow implementation
    void Render() override;
    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    void SetImGuiContext(uintptr_t ctx) override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    void OnOpen() override;
    void OnClose() override;
    // Persistence folders and files under %APPDATA%\bakkesmod\bakkesmod\data
    void EnsureDataDirectories() const;
    std::filesystem::path GetDataRoot() const;
    std::filesystem::path GetSuiteTrainingDir() const;

    // Workshop persistence API
    void LoadWorkshopMaps();
    void DiscoverWorkshopInDir(const std::filesystem::path& dir);
    std::filesystem::path GetWorkshopLoaderConfigPath() const;
    std::filesystem::path ResolveConfiguredWorkshopRoot() const;

    // lifecycle
    void onLoad() override;
    void onUnload() override;

    // settings UI (PluginSettingsWindow)
    void RenderSettings() override;

    // hooks
    void LoadHooks();
    void GameEndedEvent(std::string name);
    void TryHealCurrentPack(GameWrapper* gw); // Pack healer helper

    // Training Pack update integration
    std::filesystem::path GetTrainingPacksPath() const;
    void UpdateTrainingPackList();
    void LoadTrainingPacksFromFile(const std::filesystem::path& filePath);
    bool IsTrainingPackCacheStale() const;
    std::string FormatLastUpdatedTime() const;

    bool IsEnabled() const;
    bool IsAutoQueueEnabled() const;
    bool IsTrainingGameSpeedFixEnabled() const;
    int GetMapType() const;
    int GetDelayQueueSec() const;
    int GetDelayFreeplaySec() const;
    int GetDelayTrainingSec() const;
    int GetDelayWorkshopSec() const;
    std::string GetCurrentFreeplayCode() const;
    std::string GetCurrentTrainingCode() const;
    std::string GetCurrentWorkshopPath() const;

  private:
    void LoadTrainingGameSpeedHooks();
    void UnloadTrainingGameSpeedHooks();
    void ApplyTrainingGameSpeedFromMenuValue(float menuValue);
    static float ConvertMenuPercentToDecimal(float menuValue);

    // Loadout management
    std::unique_ptr<LoadoutManager> loadoutManager;
    std::unique_ptr<PackUsageTracker> usageTracker;
    std::shared_ptr<WorkshopDownloader> workshopDownloader;
    std::unique_ptr<TextureDownloader> textureDownloader;

    std::unique_ptr<MapManager> mapManager;
    std::unique_ptr<SettingsSync> settingsSync;
    std::unique_ptr<AutoLoadFeature> autoLoadFeature;
    std::unique_ptr<TrainingPackManager> trainingPackMgr;
    std::unique_ptr<SettingsUI> settingsUI;
    std::shared_ptr<TrainingPackUI> trainingPackUI;
    std::unique_ptr<LoadoutUI> loadoutUI;

    bool isBrowserOpen = false;
    float officialTrainingGameSpeed = 1.0f;
    uintptr_t imgui_ctx = 0;
    ImFont* clockFont = nullptr;
    ImFont* uiFont = nullptr;   // Roboto-Medium 14px
    ImFont* iconFont = nullptr; // FA5 Solid 14px (separate; BakkasMod ignores MergeMode)
    std::atomic<bool> isRenderingSettings{false};
    std::thread textureDownloadThread; // Managed texture download thread

    // Hotkey capture state
    int captureRow = -1;
    int captureSlot = 0;

    // Tracks currently-held key names (UE3 strings, e.g. "XboxTypeS_X") via HandleKeyPress hook.
    // Updated on every press/release event — never polled. Used for combo key checks in notifiers.
    std::set<std::string> heldKeys;

    // Hotkey handlers
};

struct HandleKeyPressParams
{
    int ControllerId;
    int KeyIndex;
    int KeyNumber;
    unsigned char EventType;
    unsigned char Padding[3];
    float AmountDepressed;
    unsigned int bGamepad;
};
```

## File: src/ui/ConstantsUI.h
```c
#pragma once
#include "IMGUI/imgui.h"

// Centralized UI constants for SuiteSpot plugin
// Each constant is named specifically for the UI element it controls
// for maximum granularity and ease of individual component editing

namespace UI {

// ===================================================================
// GLOBAL UI CONSTANTS
// ===================================================================

// Font scale applied to all SuiteSpot windows (per-window, not global)
constexpr float FONT_SCALE = 1.12f;

// === Interactive Control Styling ===

// Border thickness (px) on buttons, checkboxes, inputs, sliders, combos
constexpr float INTERACTIVE_FRAME_BORDER_SIZE = 1.0f;

// Corner rounding (px) on all framed interactive controls
constexpr float INTERACTIVE_FRAME_ROUNDING = 3.0f;

// Border color for interactive controls — soft blue accent
inline const ImVec4 INTERACTIVE_BORDER_COLOR = ImVec4(0.40f, 0.55f, 0.80f, 0.85f);

// Padding inside framed controls (buttons, inputs, combos) — wider than ImGui default (4,3)
inline const ImVec2 FRAME_PADDING = {8.0f, 4.0f};

// Spacing between consecutive items — more vertical breathing room than default (8,4)
inline const ImVec2 ITEM_SPACING = {8.0f, 6.0f};

// Inner padding of child/popup windows — slightly more horizontal than default (8,8)
inline const ImVec2 WINDOW_PADDING = {10.0f, 8.0f};

// Corner rounding (px) for child panel borders — matches FrameRounding
constexpr float CHILD_ROUNDING = 4.0f;

// Border size for child windows (panels, sub-panels) — separate from FrameBorderSize
constexpr float CHILD_BORDER_SIZE = 1.0f;

// Corner rounding for tab items
constexpr float TAB_ROUNDING = 3.0f;

// Corner rounding for slider/scrollbar grab handles
constexpr float GRAB_ROUNDING = 2.0f;

// Minimum pixel size for slider grab handles
constexpr float GRAB_MIN_SIZE = 10.0f;

// Corner rounding for scrollbar handles
constexpr float SCROLLBAR_ROUNDING = 3.0f;

// Inner spacing inside composed widgets (checkbox+label, combo+label etc.)
inline const ImVec2 ITEM_INNER_SPACING = {6.0f, 4.0f};

// Selectable text alignment — vertically centered
inline const ImVec2 SELECTABLE_TEXT_ALIGN = {0.0f, 0.5f};

// Controlled indent spacing for sub-sections
constexpr float INDENT_SPACING = 14.0f;

// Status warning color (orange — "no map selected", missing inputs)
inline const ImVec4 STATUS_WARN_COLOR = ImVec4(1.0f, 0.65f, 0.0f, 1.0f);

// Section header accent color — bright blue, used by DrawSectionHeader helper
inline const ImVec4 SECTION_HEADER_COLOR = ImVec4(0.45f, 0.78f, 1.0f, 1.0f);

// Active/pressed map mode button highlight
inline const ImVec4 MAP_MODE_ACTIVE_COLOR = ImVec4(0.22f, 0.50f, 0.82f, 1.00f);

// === Scrollbar styling ===
inline const ImVec4 SCROLLBAR_BG_COLOR = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
inline const ImVec4 SCROLLBAR_GRAB_COLOR = ImVec4(0.22f, 0.28f, 0.40f, 1.00f);
inline const ImVec4 SCROLLBAR_GRAB_HOVER_COLOR = ImVec4(0.30f, 0.40f, 0.58f, 1.00f);
inline const ImVec4 SCROLLBAR_GRAB_ACTIVE_COLOR = ImVec4(0.38f, 0.52f, 0.72f, 1.00f);

// Text selection highlight in inputs
inline const ImVec4 TEXT_SELECTED_BG_COLOR = ImVec4(0.20f, 0.40f, 0.70f, 0.40f);

// Popup/dropdown background — darker for depth
inline const ImVec4 POPUP_BG_COLOR = ImVec4(0.08f, 0.09f, 0.13f, 0.96f);

// Keyboard navigation highlight
inline const ImVec4 NAV_HIGHLIGHT_COLOR = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);

// Separator line color — soft accent blue
inline const ImVec4 SEPARATOR_COLOR = ImVec4(0.28f, 0.45f, 0.65f, 0.70f);

// --- Button colors (visible dark-blue-slate so buttons stand out from window bg) ---
inline const ImVec4 BUTTON_COLOR = ImVec4(0.22f, 0.28f, 0.40f, 0.80f);
inline const ImVec4 BUTTON_HOVER_COLOR = ImVec4(0.30f, 0.40f, 0.58f, 0.90f);
inline const ImVec4 BUTTON_ACTIVE_COLOR = ImVec4(0.38f, 0.52f, 0.72f, 1.00f);

// --- Frame background colors for inputs, checkboxes, combos ---
inline const ImVec4 FRAME_BG_COLOR = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
inline const ImVec4 FRAME_BG_HOVER_COLOR = ImVec4(0.22f, 0.26f, 0.34f, 1.00f);
inline const ImVec4 FRAME_BG_ACTIVE_COLOR = ImVec4(0.28f, 0.34f, 0.46f, 1.00f);

// CheckMark color drawn inside checkboxes
inline const ImVec4 CHECKMARK_COLOR = ImVec4(0.65f, 0.88f, 1.00f, 1.00f);

// --- Selectable / list-row highlight colors (Header in ImGui parlance) ---
inline const ImVec4 HEADER_COLOR = ImVec4(0.25f, 0.40f, 0.60f, 0.65f);
inline const ImVec4 HEADER_HOVER_COLOR = ImVec4(0.30f, 0.48f, 0.68f, 0.75f);
inline const ImVec4 HEADER_ACTIVE_COLOR = ImVec4(0.35f, 0.55f, 0.78f, 1.00f);

// --- Tab colors ---
inline const ImVec4 TAB_COLOR = ImVec4(0.16f, 0.18f, 0.24f, 0.90f);
inline const ImVec4 TAB_HOVER_COLOR = ImVec4(0.28f, 0.38f, 0.52f, 1.00f);
inline const ImVec4 TAB_ACTIVE_COLOR = ImVec4(0.22f, 0.32f, 0.48f, 1.00f);
inline const ImVec4 TAB_UNFOCUSED_COLOR = ImVec4(0.12f, 0.14f, 0.18f, 0.80f);
inline const ImVec4 TAB_UNFOCUSED_ACTIVE_COLOR = ImVec4(0.18f, 0.24f, 0.36f, 1.00f);

// Child window background — slightly darker than outer window for depth
inline const ImVec4 CHILD_BG_COLOR = ImVec4(0.10f, 0.11f, 0.15f, 0.60f);

// ===================================================================
// SETTINGS UI CONSTANTS
// ===================================================================
namespace SettingsUI {

// === Dropdown Widths ===

// Width of the "Freeplay Maps" dropdown in Map Selection tab
constexpr float FREEPLAY_MAPS_DROPDOWN_WIDTH = 260.0f;

// Width of the "Training Packs" dropdown in Map Selection tab
constexpr float TRAINING_PACKS_DROPDOWN_WIDTH = 260.0f;

// Width of the "Workshop Maps" dropdown in Map Selection tab
constexpr float WORKSHOP_MAPS_DROPDOWN_WIDTH = 260.0f;

// === Input Box Widths ===

// Width of the "Delay Freeplay (sec)" input box in Map Selection tab
constexpr float DELAY_FREEPLAY_INPUT_WIDTH = 220.0f;

// Width of the "Delay Training (sec)" input box in Map Selection tab
constexpr float DELAY_TRAINING_INPUT_WIDTH = 220.0f;

// Width of the "Delay Workshop (sec)" input box in Map Selection tab
constexpr float DELAY_WORKSHOP_INPUT_WIDTH = 220.0f;

// Width of the "Delay Queue (sec)" input box in Auto-Queue tab
constexpr float DELAY_QUEUE_INPUT_WIDTH = 220.0f;

// Width of the "Workshop maps root folder" input in Workshop Source section
constexpr float WORKSHOP_PATH_INPUT_WIDTH = 420.0f;

// === Range Limits (Min/Max Values) ===

// Minimum value for Freeplay delay slider (seconds)
constexpr int DELAY_FREEPLAY_MIN_SECONDS = 0;

// Maximum value for Freeplay delay slider (seconds)
constexpr int DELAY_FREEPLAY_MAX_SECONDS = 300;

// Minimum value for Training delay slider (seconds)
constexpr int DELAY_TRAINING_MIN_SECONDS = 0;

// Maximum value for Training delay slider (seconds)
constexpr int DELAY_TRAINING_MAX_SECONDS = 300;

// Minimum value for Workshop delay slider (seconds)
constexpr int DELAY_WORKSHOP_MIN_SECONDS = 0;

// Maximum value for Workshop delay slider (seconds)
constexpr int DELAY_WORKSHOP_MAX_SECONDS = 300;

// Minimum value for Queue delay slider (seconds)
constexpr int DELAY_QUEUE_MIN_SECONDS = 0;

// Maximum value for Queue delay slider (seconds)
constexpr int DELAY_QUEUE_MAX_SECONDS = 300;

// === Positions and Spacing ===

// Horizontal position for status text display in General tab
constexpr float STATUS_TEXT_POSITION_X = 420.0f;

// Spacing between radio buttons in Map Type selection
constexpr float MAP_TYPE_RADIO_BUTTON_SPACING = 16.0f;

// === Status Colors ===

// Text color for section headers in settings (brightened from 0.6)
inline const ImVec4 HEADER_TEXT_COLOR = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

// Color for separator line in status section (white)
inline const ImVec4 STATUS_SEPARATOR_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

// Text color for "Enabled" status in Auto-Load settings (bright green)
inline const ImVec4 STATUS_ENABLED_TEXT_COLOR = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);

// Text color for "Disabled" status in Auto-Load settings (bright red)
inline const ImVec4 STATUS_DISABLED_TEXT_COLOR = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

// === Timer Durations ===

// Duration (seconds) for success message fadeout after adding custom training pack
constexpr float CUSTOM_PACK_SUCCESS_MESSAGE_DURATION = 3.0f;

// Duration (seconds) for error message fadeout in workshop path validation
constexpr float WORKSHOP_PATH_ERROR_MESSAGE_DURATION = 2.0f;

// Divisor used to calculate alpha fade effect for success messages (timer / divisor)
constexpr float SUCCESS_MESSAGE_FADE_DIVISOR = 3.0f;

// === Message Colors ===

// Text color for success message "Pack added!" in custom training form (green)
inline const ImVec4 CUSTOM_PACK_SUCCESS_TEXT_COLOR = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

// === Hotkeys Tab ===

// Width of the label column in the Hotkeys tab
constexpr float HOTKEY_LABEL_COL_WIDTH = 150.0f;

// Width of each key name input field in the Hotkeys tab
constexpr float HOTKEY_KEY_INPUT_WIDTH = 110.0f;

struct HotkeyRow
{
    const char* label;
    const char* key1CVar;
    const char* key2CVar;
};

static const HotkeyRow HOTKEY_ROWS[] = {
    {"Cycle Mode Fwd", "suitespot_hotkey_map_mode_fwd_key", "suitespot_hotkey_map_mode_fwd_key2"},
    {"Cycle Mode Back", "suitespot_hotkey_map_mode_bk_key", "suitespot_hotkey_map_mode_bk_key2"},
    {"Cycle Map Fwd", "suitespot_hotkey_cycle_map_fwd_key", "suitespot_hotkey_cycle_map_fwd_key2"},
    {"Cycle Map Back", "suitespot_hotkey_cycle_map_bk_key", "suitespot_hotkey_cycle_map_bk_key2"},
    {"Load Now", "suitespot_hotkey_load_now_key", "suitespot_hotkey_load_now_key2"},
};

} // namespace SettingsUI

// ===================================================================
// TRAINING PACK UI CONSTANTS
// ===================================================================
namespace TrainingPackUI {

// === Filter Layout - Responsive Widths ===

// Percentage of available width for Search filter box in Training Packs tab (40%)
constexpr float FILTER_SEARCH_WIDTH_PERCENT = 0.40f;

// Percentage of available width for Difficulty filter dropdown in Training Packs tab (25%)
constexpr float FILTER_DIFFICULTY_WIDTH_PERCENT = 0.25f;

// Percentage of available width for Shots filter in Training Packs tab (35%)
constexpr float FILTER_SHOTS_WIDTH_PERCENT = 0.35f;

// Minimum pixel width for Search filter box (used when window is too narrow)
constexpr float FILTER_SEARCH_MIN_WIDTH = 150.0f;

// Minimum pixel width for Difficulty filter dropdown (used when window is too narrow)
constexpr float FILTER_DIFFICULTY_MIN_WIDTH = 120.0f;

// Minimum pixel width for Shots filter (used when window is too narrow)
constexpr float FILTER_SHOTS_MIN_WIDTH = 150.0f;

// === Dropdowns ===

// Width of the tag filter dropdown in Training Packs tab
constexpr float TAG_FILTER_DROPDOWN_WIDTH = 200.0f;

// === Positions and Spacing ===

// Button group width offset from right edge of window
constexpr float BUTTON_GROUP_OFFSET_FROM_RIGHT = 280.0f;

// Indentation amount for custom pack form section
constexpr float CUSTOM_PACK_FORM_INDENT = 10.0f;

// === Table Configuration ===

// Number of columns in Training Packs table
constexpr int TABLE_COLUMN_COUNT = 5;

// Minimum width for any column in Training Packs table (before dynamic sizing)
constexpr float TABLE_MIN_COLUMN_WIDTH = 40.0f;

// Extra padding added to calculated column widths in Training Packs table
constexpr float TABLE_COLUMN_PADDING = 20.0f;

// Default width for Actions column in Training Packs table
constexpr float TABLE_ACTIONS_COLUMN_WIDTH = 200.0f;

// Maximum width cap for Name column in Training Packs table
constexpr float TABLE_NAME_COLUMN_MAX_WIDTH = 400.0f;

// === Custom Pack Form - Input Widths ===

// Width of "Training Map Code" input field in custom pack form
constexpr float CUSTOM_PACK_CODE_INPUT_WIDTH = 220.0f;

// Width of "Training Map Name" input field in custom pack form
constexpr float CUSTOM_PACK_NAME_INPUT_WIDTH = 300.0f;

// Width of "Creator" input field in custom pack form
constexpr float CUSTOM_PACK_CREATOR_INPUT_WIDTH = 200.0f;

// Width of "Tags" input field in custom pack form
constexpr float CUSTOM_PACK_TAGS_INPUT_WIDTH = 300.0f;

// Width of "Video URL" input field in custom pack form
constexpr float CUSTOM_PACK_VIDEO_URL_INPUT_WIDTH = 350.0f;

// Width of "Notes" multiline text area in custom pack form
constexpr float CUSTOM_PACK_NOTES_INPUT_WIDTH = 400.0f;

// Height of "Notes" multiline text area in custom pack form
constexpr float CUSTOM_PACK_NOTES_INPUT_HEIGHT = 60.0f;

// === Custom Pack Form - Dropdown Widths ===

// Width of the difficulty dropdown in custom pack form
constexpr float CUSTOM_PACK_DIFFICULTY_DROPDOWN_WIDTH = 150.0f;

// === Custom Pack Form - Button Sizes ===

// Width of "Add Pack" button in custom pack form
constexpr float CUSTOM_PACK_ADD_BUTTON_WIDTH = 100.0f;

// Height of "Add Pack" button in custom pack form (0 = auto)
constexpr float CUSTOM_PACK_ADD_BUTTON_HEIGHT = 0.0f;

// Width of "Clear" button in custom pack form
constexpr float CUSTOM_PACK_CLEAR_BUTTON_WIDTH = 80.0f;

// Height of "Clear" button in custom pack form (0 = auto)
constexpr float CUSTOM_PACK_CLEAR_BUTTON_HEIGHT = 0.0f;

// === Shot Count Ranges ===

// Minimum value for "Min Shots" filter slider in Training Packs tab
constexpr int FILTER_MIN_SHOTS_MIN = 0;

// Maximum value for "Min Shots" filter slider in Training Packs tab
constexpr int FILTER_MIN_SHOTS_MAX = 50;

// Minimum value for shot count slider in custom pack creation form
constexpr int CUSTOM_PACK_SHOTS_MIN = 1;

// Maximum value for shot count slider in custom pack creation form
constexpr int CUSTOM_PACK_SHOTS_MAX = 50;

// === Validation Constants ===

// Expected character length for properly formatted training pack code
constexpr int PACK_CODE_EXPECTED_LENGTH = 19;

// Position of first dash in training pack code (0-indexed)
constexpr int PACK_CODE_DASH_POSITION_1 = 4;

// Position of second dash in training pack code (0-indexed)
constexpr int PACK_CODE_DASH_POSITION_2 = 9;

// Position of third dash in training pack code (0-indexed)
constexpr int PACK_CODE_DASH_POSITION_3 = 14;

// Maximum characters allowed for raw pack code before formatting
constexpr int PACK_CODE_RAW_MAX_LENGTH = 16;

// === Difficulty Badge Colors ===

// Background color for Unranked/Unknown difficulty badge
inline const ImVec4 DIFFICULTY_BADGE_UNRANKED_COLOR = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);

// Background color for Bronze difficulty badge (#925732)
inline const ImVec4 DIFFICULTY_BADGE_BRONZE_COLOR = ImVec4(0.57f, 0.34f, 0.20f, 1.0f);

// Background color for Silver difficulty badge (#91969B)
inline const ImVec4 DIFFICULTY_BADGE_SILVER_COLOR = ImVec4(0.57f, 0.59f, 0.61f, 1.0f);

// Background color for Gold difficulty badge (#C3A11C)
inline const ImVec4 DIFFICULTY_BADGE_GOLD_COLOR = ImVec4(0.76f, 0.63f, 0.11f, 1.0f);

// Background color for Platinum difficulty badge (#43F8F0)
inline const ImVec4 DIFFICULTY_BADGE_PLATINUM_COLOR = ImVec4(0.26f, 0.97f, 0.94f, 1.0f);

// Background color for Diamond difficulty badge (#00BBFF)
inline const ImVec4 DIFFICULTY_BADGE_DIAMOND_COLOR = ImVec4(0.00f, 0.73f, 1.00f, 1.0f);

// Background color for Champion difficulty badge (#990099)
inline const ImVec4 DIFFICULTY_BADGE_CHAMPION_COLOR = ImVec4(0.60f, 0.00f, 0.60f, 1.0f);

// Background color for Grand Champion difficulty badge (#FC272F)
inline const ImVec4 DIFFICULTY_BADGE_GRAND_CHAMPION_COLOR = ImVec4(0.99f, 0.15f, 0.18f, 1.0f);

// Background color for Supersonic Legend difficulty badge (#F6FAFF)
inline const ImVec4 DIFFICULTY_BADGE_SUPERSONIC_LEGEND_COLOR = ImVec4(0.96f, 0.98f, 1.00f, 1.0f);

// === UI State Colors ===

// Text color for section headers in Training Packs tab (bright cyan)
inline const ImVec4 SECTION_HEADER_TEXT_COLOR = ImVec4(0.4f, 0.9f, 1.0f, 1.0f);

// Text color for "Last updated" timestamp (brightened gray)
inline const ImVec4 LAST_UPDATED_TEXT_COLOR = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

// Text color for scraping status message (bright yellow)
inline const ImVec4 SCRAPING_STATUS_TEXT_COLOR = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);

// Text color for success message "Pack added!" in custom pack form (bright green)
inline const ImVec4 SUCCESS_MESSAGE_TEXT_COLOR = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);

// Text color for error message in pack code validation (bright red)
inline const ImVec4 ERROR_MESSAGE_TEXT_COLOR = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

// Text color for disabled/info text in custom pack form (brightened gray)
inline const ImVec4 DISABLED_INFO_TEXT_COLOR = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

// === Button Colors ===

// Background color for "In Bag" badge indicator (green)
inline const ImVec4 IN_BAG_BUTTON_BG_COLOR = ImVec4(0.2f, 0.6f, 0.2f, 1.0f);

// Background color for "Delete" button in Training Packs table (red)
inline const ImVec4 DELETE_BUTTON_BG_COLOR = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);

// Background color for "Delete" button when hovered (lighter red)
inline const ImVec4 DELETE_BUTTON_HOVER_COLOR = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);

} // namespace TrainingPackUI

// ===================================================================
// LOADOUT UI CONSTANTS
// ===================================================================
namespace LoadoutUI {

// === Dropdown Widths ===

// Width of the loadout selection dropdown in Loadout Manager tab
constexpr float LOADOUT_SELECTOR_DROPDOWN_WIDTH = 220.0f;

// === Timer Durations ===

// Duration (seconds) for "Applying loadout..." status message
constexpr float APPLYING_STATUS_DURATION = 5.0f;

// Duration (seconds) for "Loadout applied" success message
constexpr float SUCCESS_MESSAGE_DURATION = 3.0f;

// Duration (seconds) for "Loadouts refreshed" message
constexpr float REFRESH_MESSAGE_DURATION = 2.5f;

// === Status Message Colors ===

// Text color for section header in Loadout Manager tab (bright cyan)
inline const ImVec4 SECTION_HEADER_COLOR = ImVec4(0.4f, 0.9f, 1.0f, 1.0f);

// Text color for error/warning messages (bright red)
inline const ImVec4 ERROR_WARNING_TEXT_COLOR = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);

// Text color for "Applying loadout..." status (yellow)
inline const ImVec4 APPLYING_STATUS_COLOR = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

// Text color for success message "Loadout applied" (green)
inline const ImVec4 SUCCESS_MESSAGE_COLOR = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);

// Text color for "Loadouts refreshed" message (blue)
inline const ImVec4 REFRESH_MESSAGE_COLOR = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);

} // namespace LoadoutUI

// ===================================================================
// QUICK PICKS UI CONSTANTS (Two-Panel Layout)
// ===================================================================
namespace QuickPicksUI {
// Percentage of available width for left panel (pack list)
constexpr float LEFT_PANEL_WIDTH_PERCENT = 0.40f;

// Minimum width for left panel
constexpr float LEFT_PANEL_MIN_WIDTH = 200.0f;

// Height of the two-panel browser area
constexpr float BROWSER_HEIGHT = 400.0f;

// Colors
inline const ImVec4 PACK_NAME_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 CREATOR_COLOR = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
inline const ImVec4 DESCRIPTION_COLOR = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
inline const ImVec4 SELECTED_BADGE_COLOR = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
} // namespace QuickPicksUI

// ===================================================================
// WORKSHOP BROWSER UI CONSTANTS (Two-Panel Layout)
// ===================================================================
namespace WorkshopBrowserUI {
// Percentage of available width for left panel (map list)
constexpr float LEFT_PANEL_WIDTH_PERCENT = 0.40f;

// Minimum width for left panel
constexpr float LEFT_PANEL_MIN_WIDTH = 200.0f;

// Height of the two-panel browser area
constexpr float BROWSER_HEIGHT = 400.0f;

// Preview image dimensions
constexpr float PREVIEW_IMAGE_WIDTH = 280.0f;
constexpr float PREVIEW_IMAGE_HEIGHT = 158.0f; // 16:9 aspect ratio

// Colors
inline const ImVec4 MAP_NAME_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 AUTHOR_COLOR = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
inline const ImVec4 DESCRIPTION_COLOR = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
inline const ImVec4 NO_MAPS_COLOR = ImVec4(0.8f, 0.8f, 0.3f, 1.0f);
inline const ImVec4 SELECTED_BADGE_COLOR = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
} // namespace WorkshopBrowserUI

// ===================================================================
// PACK BROWSER UI CONSTANTS (Two-Panel Layout — floating window)
// ===================================================================
namespace PackBrowserUI {
// Left panel takes 60% of available width (wider list to show names)
constexpr float LEFT_PANEL_WIDTH_PERCENT = 0.60f;

// Minimum pixel width for left panel before clamping
constexpr float LEFT_PANEL_MIN_WIDTH = 320.0f;

// Height of the two-panel browser area (shrinks to allow header/footer)
constexpr float BROWSER_HEIGHT = -60.0f; // negative = fill to bottom minus N px

// YouTube thumbnail dimensions (standard mqdefault.jpg)
constexpr float THUMBNAIL_WIDTH = 280.0f;
constexpr float THUMBNAIL_HEIGHT = 157.5f; // 16:9

// Action button height in detail panel
constexpr float ACTION_BUTTON_HEIGHT = 26.0f;

// Tag badge padding (horizontal, vertical)
inline const ImVec2 TAG_BADGE_PADDING = ImVec2(6.0f, 2.0f);

// Colors — detail panel
inline const ImVec4 PACK_NAME_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline const ImVec4 CREATOR_COLOR = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
inline const ImVec4 STATS_COLOR = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
inline const ImVec4 COMMENTS_COLOR = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
inline const ImVec4 SELECTED_BADGE_COLOR = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
inline const ImVec4 TAG_BADGE_COLOR = ImVec4(0.25f, 0.40f, 0.65f, 1.0f);    // dark blue chip bg
inline const ImVec4 TAG_TEXT_COLOR = ImVec4(0.75f, 0.88f, 1.00f, 1.0f);     // light blue text
inline const ImVec4 WATCH_BUTTON_COLOR = ImVec4(0.65f, 0.10f, 0.10f, 1.0f); // YouTube red
inline const ImVec4 WATCH_BUTTON_HOVER = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
inline const ImVec4 NO_SELECTION_COLOR = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
} // namespace PackBrowserUI

} // namespace UI
```

## File: src/ui/HelpersUI.cpp
```cpp
#include "pch.h"
#include "HelpersUI.h"
#include <algorithm> // for std::clamp

namespace UI {
namespace Helpers {

//
// InputIntWithRange - Smart number input with automatic validation and saving
//
bool InputIntWithRange(const char* label, int& value, int minValue, int maxValue, float width, const char* cvarName,
                       std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                       const char* tooltip, const char* rangeHint, ImGuiInputTextFlags extraFlags)
{
    // Set how wide the input box should be (if a width was specified)
    // This is like saying "make this input box 220 pixels wide"
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }

    // Show the actual input box and check if the user changed the number
    bool changed = ImGui::InputInt(label, &value, 1, 100, extraFlags);
    if (changed) {
        // User typed a new number - make sure it's in the valid range
        // Like making sure someone can't set a delay to -50 seconds or 9999 seconds
        value = std::clamp(value, minValue, maxValue);

        // Save the new value to the plugin's settings file
        // This is what makes the setting persist when you restart the game
        if (cvarName && cvarManager && gameWrapper) {
            SetCVarSafely(cvarName, value, cvarManager, gameWrapper);

            // Immediately persist to config file to prevent settings loss on crash
            cvarManager->executeCommand("writeconfig", false);
        }
    }

    // Show the range hint next to the input (like "0-300s")
    // This reminds users what values are allowed
    if (rangeHint) {
        ImGui::SameLine();
        ImGui::TextDisabled(rangeHint);
    }

    // If there's a tooltip, show it when the user hovers their mouse
    // This is the "helpful hint" feature
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    return changed;
}

//
// ComboWithTooltip - Dropdown menu with automatic help tooltip
//
bool ComboWithTooltip(const char* label, const char* previewValue, const char* tooltip, float width)
{
    // Set how wide the dropdown should be (if a width was specified)
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }

    // Start the dropdown menu
    // BeginCombo returns true if the dropdown is open (user clicked on it)
    bool isOpen = ImGui::BeginCombo(label, previewValue);

    // Show tooltip when hovering over the dropdown
    // This works whether the dropdown is open or closed
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the dropdown is open
    // If true, the caller needs to add items with ImGui::Selectable() and call ImGui::EndCombo()
    return isOpen;
}

//
// ButtonWithTooltip - Button with automatic help tooltip
//
bool ButtonWithTooltip(const char* label, const char* tooltip, const ImVec2& size)
{
    // Create the button and check if it was clicked
    bool clicked = ImGui::Button(label, size);

    // Show tooltip when hovering over the button
    // This lets users know what the button does before clicking
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the button was clicked
    return clicked;
}

//
// ShowStatusMessage - Auto-fading status message
//
void ShowStatusMessage(const std::string& text, const ImVec4& color, float& timer, float deltaTime)
{
    // Only show the message if there's time remaining on the timer
    // Once timer hits 0, the message disappears
    if (timer > 0.0f && !text.empty()) {
        // Add some spacing before the message for visual separation
        ImGui::Spacing();

        // Display the message in the specified color
        // %s means "insert the text here"
        ImGui::TextColored(color, "%s", text.c_str());

        // Count down the timer using the time that passed since last frame
        // deltaTime is usually 0.016 seconds (1/60th of a second at 60 FPS)
        timer -= deltaTime;

        // If timer went negative, clamp it to exactly 0
        // This prevents weird negative timer values
        if (timer <= 0.0f) {
            timer = 0.0f;
        }
    }
}

//
// ShowStatusMessageWithFade - Status message with smooth transparency fade
//
void ShowStatusMessageWithFade(const std::string& text, const ImVec4& baseColor, float& timer, float maxDuration,
                               float deltaTime)
{
    // Only show the message if there's time remaining
    if (timer > 0.0f && !text.empty()) {
        // Add some spacing for visual separation
        ImGui::Spacing();

        // Calculate how transparent the text should be based on remaining time
        // If timer = maxDuration (full time left), alpha = 1.0 (fully visible)
        // If timer = 0 (no time left), alpha = 0.0 (fully transparent)
        // This creates a smooth fade-out effect
        float alpha = timer / maxDuration;

        // Create a faded version of the color with the calculated alpha
        // Keep RGB the same, just change the transparency
        ImVec4 fadedColor = ImVec4(baseColor.x, baseColor.y, baseColor.z, alpha);

        // Display the message with the faded color
        ImGui::TextColored(fadedColor, "%s", text.c_str());

        // Count down the timer
        timer -= deltaTime;

        // Clamp timer to 0 if it went negative
        if (timer <= 0.0f) {
            timer = 0.0f;
        }
    }
}

//
// CheckboxWithCVar - Checkbox that automatically saves to settings
//
bool CheckboxWithCVar(const char* label, bool& value, const char* cvarName,
                      std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                      const char* tooltip)
{
    // Create the checkbox and check if it was toggled
    bool toggled = ImGui::Checkbox(label, &value);

    if (toggled) {
        // User clicked the checkbox - save the new value to settings
        // This makes the checkbox state persist when you restart the game
        if (cvarName && cvarManager && gameWrapper) {
            SetCVarSafely(cvarName, value, cvarManager, gameWrapper);

            // Immediately persist to config file to prevent settings loss on crash
            cvarManager->executeCommand("writeconfig", false);
        }
    }

    // Show tooltip when hovering over the checkbox
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the checkbox was toggled
    return toggled;
}

//
// InputTextWithTooltip - Text input box with automatic tooltip
//
bool InputTextWithTooltip(const char* label, char* buf, size_t bufSize, const char* tooltip, float width,
                          ImGuiInputTextFlags flags)
{
    // Set how wide the input box should be (if a width was specified)
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }

    // Show the text input box and check if the user modified the text
    bool changed = ImGui::InputText(label, buf, bufSize, flags);

    // Show tooltip when hovering over the input box
    // Helps users understand what to type
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Return whether the text was modified
    return changed;
}

//
// ExecuteCommandSafely - Execute command on game thread
//
void ExecuteCommandSafely(std::shared_ptr<GameWrapper> gameWrapper, std::shared_ptr<CVarManagerWrapper> cvarManager,
                          const std::string& command, float delay)
{
    // Safety checks
    if (!gameWrapper || !cvarManager) return;

    // Schedule command execution on game thread to avoid crashes
    gameWrapper->SetTimeout([cvarManager, command](GameWrapper* gw) { cvarManager->executeCommand(command); }, delay);
}

} // namespace Helpers
} // namespace UI
```

## File: src/ui/HelpersUI.h
```c
#pragma once

// UIHelpers.h
//
// This file contains "helper functions" that make the UI code cleaner.
// Instead of writing the same 5-10 lines of code over and over for every button or input box,
// we can call one of these helpers that does all the work in one line.
//
// Think of it like having pre-made "templates" for common UI elements:
// - Input boxes that automatically validate numbers and save settings
// - Buttons that show helpful tooltips when you hover
// - Dropdown menus with tooltips
// - Status messages that fade out automatically
//
// This makes the code easier to read, maintain, and less error-prone.

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "IMGUI/imgui.h"
#include <string>

namespace UI {
namespace Helpers {

//
// SetCVarSafely - Safely saves a setting value to the plugin
//
// This is a utility function that saves values to BakkesMod's settings system (CVars).
// It includes safety checks to prevent crashes if the settings system isn't available.
//
// Think of CVars like a settings file - when you change a value in the UI, this saves it
// so it persists when you restart Rocket League.
//
// Parameters:
//   cvarName - The internal name of the setting (e.g., "suitespot_delay_queue_sec")
//   value - The value to save (can be int, bool, float, string, etc.)
//   cvarManager - The plugin's settings manager (handles the actual saving)
//
template <typename T>
void SetCVarSafely(const char* cvarName, const T& value, std::shared_ptr<CVarManagerWrapper> cvarManager,
                   std::shared_ptr<GameWrapper> gameWrapper)
{
    // Safety check: Make sure the managers exist
    if (!cvarManager || !gameWrapper) return;

    // Get the setting
    auto cvar = cvarManager->getCvar(cvarName);
    if (cvar) {
        // CRITICAL: Force the update to the Game Thread
        // This prevents "Rendering thread exception" when called from UI/Render threads
        gameWrapper->SetTimeout(
            [cvar, value](GameWrapper* gw) {
                // Use a copy of the value and the cvar wrapper inside the lambda
                CVarWrapper(cvar).setValue(value);
            },
            0.0f);
    }
}

//
// InputIntWithRange - Creates a "smart" number input box
//
bool InputIntWithRange(const char* label, int& value, int minValue, int maxValue, float width, const char* cvarName,
                       std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                       const char* tooltip = nullptr, const char* rangeHint = nullptr,
                       ImGuiInputTextFlags extraFlags = 0);

//
// ComboWithTooltip - Creates a dropdown menu with automatic tooltip
//
bool ComboWithTooltip(const char* label, const char* previewValue, const char* tooltip, float width = 0.0f);

//
// ButtonWithTooltip - Creates a button with automatic tooltip
//
bool ButtonWithTooltip(const char* label, const char* tooltip, const ImVec2& size = ImVec2(0, 0));

//
// ShowStatusMessage - Displays a message that automatically fades out
//
void ShowStatusMessage(const std::string& text, const ImVec4& color, float& timer, float deltaTime);

//
// ShowStatusMessageWithFade - Status message with smooth alpha fade-out
//
void ShowStatusMessageWithFade(const std::string& text, const ImVec4& baseColor, float& timer, float maxDuration,
                               float deltaTime);

//
// CheckboxWithCVar - Creates a checkbox that automatically saves to settings
//
bool CheckboxWithCVar(const char* label, bool& value, const char* cvarName,
                      std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper,
                      const char* tooltip = nullptr);

//
// InputTextWithTooltip - Creates a text input box with automatic tooltip
//
// Creates a text input field that automatically shows a tooltip when you hover over it.
// Cleaner than writing the input and tooltip code separately.
//
// Example: "Training Map Code" input with tooltip "Enter the code (e.g., 555F-7503-BBB9-E1E3)"
//
// Parameters:
//   label - What to display next to the input box
//   buf - Character buffer for the text being edited
//   bufSize - Size of the buffer (how many characters it can hold)
//   tooltip - Help text displayed when hovering
//   width - How wide the input box should be in pixels (0 = use default width)
//   flags - Special input options (optional - see ImGui docs)
//
// Returns: true if the text was modified
//
// Usage:
//   char mapCode[64] = "";
//   if (InputTextWithTooltip("Code", mapCode, sizeof(mapCode),
//                            "Enter pack code", 220.0f)) {
//       // Text was changed
//   }
//
bool InputTextWithTooltip(const char* label, char* buf, size_t bufSize, const char* tooltip, float width = 0.0f,
                          ImGuiInputTextFlags flags = 0);

//
// ExecuteCommandSafely - Execute BakkesMod command on game thread
//
// Safely executes a command by scheduling it on the game thread to avoid crashes.
// This is the standard way to execute commands from UI or render threads.
//
// Parameters:
//   gameWrapper - The game wrapper (for SetTimeout)
//   cvarManager - The CVar manager (for executeCommand)
//   command - The command to execute (e.g., "load_freeplay 555F-7503-BBB9-E1E3")
//   delay - Optional delay in seconds before execution (default: 0.0f)
//
// Usage:
//   ExecuteCommandSafely(gameWrapper, cvarManager, "load_freeplay ABC123");
//
void ExecuteCommandSafely(std::shared_ptr<GameWrapper> gameWrapper, std::shared_ptr<CVarManagerWrapper> cvarManager,
                          const std::string& command, float delay = 0.0f);

} // namespace Helpers
} // namespace UI
```

## File: src/ui/LoadoutUI.cpp
```cpp
#include "pch.h"

#include "LoadoutUI.h"
#include "SuiteSpot.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"

LoadoutUI::LoadoutUI(SuiteSpot* plugin) : plugin_(plugin) {}

void LoadoutUI::RenderLoadoutControls() {
    ImGui::Spacing();

    auto* loadoutManager = plugin_->loadoutManager.get();
    if (loadoutManager) {
        if (!loadoutsInitialized) {
            loadoutNames = loadoutManager->GetLoadoutNames();
            loadoutManager->GetCurrentLoadoutName([this](std::string name) {
                currentLoadoutName = name;
            });
            loadoutsInitialized = true;
        }

        ImGui::TextColored(UI::LoadoutUI::SECTION_HEADER_COLOR, "Current Loadout:");
        ImGui::SameLine();
        if (currentLoadoutName.empty()) {
            ImGui::TextUnformatted("<Unknown>");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Loadout not detected yet. Refresh to check available presets.");
            }
        } else {
            ImGui::TextUnformatted(currentLoadoutName.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Your currently equipped loadout preset");
            }
        }

        ImGui::Spacing();

        if (loadoutNames.empty()) {
            ImGui::TextColored(UI::LoadoutUI::ERROR_WARNING_TEXT_COLOR, "No loadouts found. Open Garage to create presets, then click Refresh.");
        } else {
            const char* comboLabel = (selectedLoadoutIndex >= 0 && selectedLoadoutIndex < (int)loadoutNames.size()) ?
                loadoutNames[selectedLoadoutIndex].c_str() : "<Select loadout>";
            ImGui::SetNextItemWidth(UI::LoadoutUI::LOADOUT_SELECTOR_DROPDOWN_WIDTH);
            if (ImGui::BeginCombo("##loadout_combo", comboLabel)) {
                for (int i = 0; i < (int)loadoutNames.size(); ++i) {
                    bool isSelected = (i == selectedLoadoutIndex);
                    if (ImGui::Selectable(loadoutNames[i].c_str(), isSelected)) {
                        selectedLoadoutIndex = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select a loadout preset to equip");
            }

            ImGui::SameLine();
            if (ImGui::Button("Apply Loadout")) {
                if (selectedLoadoutIndex >= 0 && selectedLoadoutIndex < (int)loadoutNames.size()) {
                    std::string selectedName = loadoutNames[selectedLoadoutIndex];
                    // Show "Applying..." feedback immediately
                    loadoutStatus.ShowWarning("Applying...", UI::LoadoutUI::APPLYING_STATUS_DURATION);

                    loadoutManager->SwitchLoadout(selectedName, [this, selectedName](bool success) {
                        if (success) {
                            currentLoadoutName = selectedName;
                            loadoutStatus.ShowSuccess("Applied \"" + selectedName + "\"",
                                UI::LoadoutUI::SUCCESS_MESSAGE_DURATION);
                        } else {
                            loadoutStatus.ShowError("Failed to apply loadout",
                                UI::LoadoutUI::SUCCESS_MESSAGE_DURATION);
                        }
                    });
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Equip the selected loadout preset");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh Loadouts")) {
            loadoutsInitialized = false;
            loadoutManager->RefreshLoadoutCache();
            loadoutNames = loadoutManager->GetLoadoutNames();
            loadoutManager->GetCurrentLoadoutName([this](std::string name) {
                currentLoadoutName = name;
            });
            selectedLoadoutIndex = 0;
            loadoutStatus.ShowInfo("Loadouts refreshed", UI::LoadoutUI::REFRESH_MESSAGE_DURATION);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Refresh the list of available loadout presets");
        }

        ImGui::Spacing();
        ImGui::TextDisabled(("Available loadouts: " + std::to_string(loadoutNames.size())).c_str());

        loadoutStatus.Render(ImGui::GetIO().DeltaTime);
    } else {
        ImGui::TextColored(UI::LoadoutUI::ERROR_WARNING_TEXT_COLOR, "LoadoutManager not initialized");
    }
}
```

## File: src/ui/LoadoutUI.h
```c
#pragma once
#include "IMGUI/imgui.h"
#include "StatusMessageUI.h"
#include <string>
#include <vector>

class SuiteSpot;

class LoadoutUI {
public:
    explicit LoadoutUI(SuiteSpot* plugin);
    void RenderLoadoutControls();

private:
    SuiteSpot* plugin_;

    UI::StatusMessage loadoutStatus;
    std::string currentLoadoutName;
    std::vector<std::string> loadoutNames;
    int selectedLoadoutIndex = 0;
    bool loadoutsInitialized = false;
};
```

## File: src/ui/SettingsUI.cpp
```cpp
#include "pch.h"

#include "SettingsUI.h"
#include "LoadoutUI.h"
#include "TrainingPackUI.h"
#include "SuiteSpot.h"
#include "MapManager.h"
#include "TrainingPackManager.h"
#include "WorkshopDownloader.h"
#include "SettingsSync.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"
#include "DefaultPacks.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"

#include "IMGUI/imguivariouscontrols.h"
#include "IMGUI/imgui_searchablecombo.h"
#include "IMGUI/SuiteSpotIcons.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>

SettingsUI::SettingsUI(SuiteSpot* plugin) : plugin_(plugin) {}

// Draws a styled section header: 3px left accent bar + colored label
static void DrawSectionHeader(const char* label, ImU32 accentColor)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accentColor), "%s", label);
}

void SettingsUI::RenderMainSettingsWindow()
{
    if (!plugin_) {
        return;
    }

    // Style vars (14)
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::INTERACTIVE_FRAME_BORDER_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, UI::INTERACTIVE_FRAME_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::FRAME_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UI::ITEM_SPACING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, UI::CHILD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, UI::CHILD_BORDER_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, UI::TAB_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, UI::GRAB_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, UI::GRAB_MIN_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, UI::SCROLLBAR_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, UI::ITEM_INNER_SPACING);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, UI::SELECTABLE_TEXT_ALIGN);
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, UI::INDENT_SPACING);
    // Colors (25)
    ImGui::PushStyleColor(ImGuiCol_Border, UI::INTERACTIVE_BORDER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button, UI::BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::BUTTON_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::BUTTON_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, UI::FRAME_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, UI::FRAME_BG_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UI::FRAME_BG_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, UI::CHECKMARK_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Header, UI::HEADER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::HEADER_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::HEADER_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Tab, UI::TAB_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, UI::TAB_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabActive, UI::TAB_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused, UI::TAB_UNFOCUSED_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, UI::TAB_UNFOCUSED_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::CHILD_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Separator, UI::SEPARATOR_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, UI::SCROLLBAR_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, UI::SCROLLBAR_GRAB_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, UI::SCROLLBAR_GRAB_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, UI::SCROLLBAR_GRAB_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, UI::TEXT_SELECTED_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, UI::POPUP_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, UI::NAV_HIGHLIGHT_COLOR);

    // Lazy-resolve uiFont (queued async in SetImGuiContext; available after first atlas build)
    if (!plugin_->uiFont) {
        auto gui = plugin_->gameWrapper->GetGUIManager();
        plugin_->uiFont = gui.GetFont("suitespot_roboto_14");
    }
    ImGui::PushFont(plugin_->uiFont); // null = default font; replaced by Roboto+FA5 once loaded
    ImGui::SetWindowFontScale(plugin_->uiFont ? 1.0f : UI::FONT_SCALE);

    // Header with metadata and Load Now button
    ImGui::BeginGroup();
    ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "By: Flicks Creations");
    std::string ver = "Version: " + std::string(plugin_version);
    ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "%s", ver.c_str());
    {
        // Format __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss") into "Mmm. dd, yyyy h:mm:ss AM/PM"
        char buildDate[] = __DATE__; // e.g. "Feb 12 2026"
        char buildTime[] = __TIME__; // e.g. "23:50:30"
        int hour = (buildTime[0] - '0') * 10 + (buildTime[1] - '0');
        const char* ampm = hour >= 12 ? "PM" : "AM";
        int hour12 = hour % 12;
        if (hour12 == 0) hour12 = 12;
        // Month is first 3 chars, day starts at index 4, year starts at index 7
        char day[3] = {buildDate[4] == ' ' ? '0' : buildDate[4], buildDate[5], '\0'};
        ImGui::TextColored(UI::SettingsUI::HEADER_TEXT_COLOR, "Built: %.3s. %s, %.4s %d:%.2s:%.2s %s", buildDate, day,
                           buildDate + 7, hour12, buildTime + 3, buildTime + 6, ampm);
    }
    ImGui::EndGroup();

    // Live system clock - drawn directly on the draw list so it doesn't affect layout
    {
        // Retrieve the clock font (queued in onLoad, built asynchronously by BakkesMod)
        if (!plugin_->clockFont) {
            auto gui = plugin_->gameWrapper->GetGUIManager();
            plugin_->clockFont = gui.GetFont("suitespot_clock_48");
        }

        ImFont* font = plugin_->clockFont;
        if (font) {
            std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_s(&local, &now);
            int h = local.tm_hour % 12;
            if (h == 0) h = 12;
            const char* ap = local.tm_hour >= 12 ? "pm" : "am";
            char clockBuf[32];
            snprintf(clockBuf, sizeof(clockBuf), "%d:%02d:%02d%s", h, local.tm_min, local.tm_sec, ap);

            float fontSize = font->FontSize; // Native size — crisp rendering
            ImVec2 clockSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, clockBuf);
            ImVec2 windowPos = ImGui::GetWindowPos();
            float windowWidth = ImGui::GetWindowWidth();
            ImVec2 groupSize = ImGui::GetItemRectSize(); // size of the preceding group
            ImVec2 groupMin = ImGui::GetItemRectMin();   // top-left of the group
            float clockX = windowPos.x + (windowWidth - clockSize.x) * 0.5f;
            float clockY = groupMin.y + (groupSize.y - clockSize.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(font, fontSize, ImVec2(clockX, clockY), IM_COL32(255, 255, 255, 255),
                                                clockBuf);
        }
    }

    {
        float btnW = ImGui::CalcTextSize("LOAD NOW").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - btnW - ImGui::GetStyle().WindowPadding.x);
    }
    if (ImGui::Button("LOAD NOW", ImVec2(0, 26))) {
        int mapType = plugin_->GetMapType();
        SuiteSpot* p = plugin_;
        bool issuedLoad = false;

        if (mapType == 0) { // Freeplay
            std::string code = p->GetCurrentFreeplayCode();
            if (!code.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Freeplay: {})", code);
                p->gameWrapper
                    ->SetTimeout([p, code](GameWrapper* gw) { p->cvarManager->executeCommand("load_freeplay " + code); },
                                 0.0f);
                statusMessage.ShowSuccess("Loading Freeplay", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        } else if (mapType == 1) { // Training
            std::string code = p->settingsSync->GetQuickPicksSelectedCode();
            if (code.empty()) code = p->GetCurrentTrainingCode();

            if (!code.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Training: {})", code);
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(code);
                p->gameWrapper
                    ->SetTimeout([p, code](GameWrapper* gw) { p->cvarManager->executeCommand("load_training " + code); },
                                 0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        } else if (mapType == 2) { // Workshop
            std::string path = p->GetCurrentWorkshopPath();
            if (!path.empty()) {
                LOG("SuiteSpot UI: User clicked Load Now (Workshop: {})", path);
                p->gameWrapper->SetTimeout(
                    [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                    0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                issuedLoad = true;
            }
        }

        if (issuedLoad && p->cvarManager) {
            p->cvarManager->executeCommand("togglemenu settings");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Immediately load the currently selected map/pack");
    }

    ImGui::Spacing();
    statusMessage.Render(ImGui::GetIO().DeltaTime);
    if (statusMessage.IsVisible()) ImGui::Spacing();

    bool enabledValue = plugin_->IsEnabled();
    int mapTypeValue = plugin_->GetMapType();
    bool autoQueueValue = plugin_->IsAutoQueueEnabled();
    int delayQueueSecValue = plugin_->GetDelayQueueSec();
    int delayFreeplaySecValue = plugin_->GetDelayFreeplaySec();
    int delayTrainingSecValue = plugin_->GetDelayTrainingSec();
    int delayWorkshopSecValue = plugin_->GetDelayWorkshopSec();
    std::string currentFreeplayCode = plugin_->GetCurrentFreeplayCode();
    std::string currentTrainingCode = plugin_->GetCurrentTrainingCode();
    std::string quickPicksSelectedCode = plugin_->settingsSync->GetQuickPicksSelectedCode();
    std::string currentWorkshopPath = plugin_->GetCurrentWorkshopPath();

    // Only show status if enabled
    if (enabledValue) {
        ImGui::Separator();

        const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
        std::string currentMap = "<none>";

        // Get current selection
        if (mapTypeValue == 0) {
            auto it = std::find_if(RLMaps.begin(), RLMaps.end(),
                                   [&](const MapEntry& e) { return e.code == currentFreeplayCode; });
            if (it != RLMaps.end()) currentMap = it->name;
        } else if (mapTypeValue == 1) {
            std::string targetCode = quickPicksSelectedCode;
            if (targetCode.empty()) targetCode = currentTrainingCode;
            auto targetPack = plugin_->trainingPackMgr->GetPackByCode(targetCode);
            if (targetPack) {
                currentMap = targetPack->name;
            } else if (!targetCode.empty()) {
                currentMap = targetCode + " (custom)";
            }
        } else if (mapTypeValue == 2) {
            auto it = std::find_if(RLWorkshop.begin(), RLWorkshop.end(),
                                   [&](const WorkshopEntry& e) { return e.filePath == currentWorkshopPath; });
            if (it != RLWorkshop.end()) currentMap = it->name;
        }

        const ImVec4 green = UI::SettingsUI::STATUS_ENABLED_TEXT_COLOR;
        const ImVec4 red = UI::SettingsUI::STATUS_DISABLED_TEXT_COLOR;
        const ImVec4 orange = UI::STATUS_WARN_COLOR;
        const ImVec4 accent = UI::SECTION_HEADER_COLOR;
        const ImVec4 dim = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        // Status bar as framed child panel (single line)
        float sbH = ImGui::GetTextLineHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + 2.0f;
        ImGui::BeginChildFrame(ImGui::GetID("##StatusBar"), ImVec2(-1.0f, sbH));
        {
            ImGui::TextColored(accent, ICON_FA_CIRCLE);
            ImGui::SameLine(0, 5);
            ImGui::Text("Mode:");
            ImGui::SameLine(0, 4);
            ImGui::TextColored(green, "%s", modeNames[mapTypeValue]);

            ImGui::SameLine(0, 10);
            ImGui::TextColored(dim, "|");
            ImGui::SameLine(0, 10);

            ImGui::Text("Map:");
            ImGui::SameLine(0, 4);
            bool noMap = (currentMap == "<none>" || currentMap == "<none selected>");
            ImGui::TextColored(noMap ? orange : green, "%s", currentMap.c_str());

            ImGui::SameLine(0, 10);
            ImGui::TextColored(dim, "|");
            ImGui::SameLine(0, 10);

            const ImVec4 queueColor = autoQueueValue ? green : red;
            ImGui::TextColored(queueColor, "Auto-Queue: %s", autoQueueValue ? "ON" : "OFF");
            if (autoQueueValue && delayQueueSecValue > 0) {
                ImGui::SameLine(0, 4);
                ImGui::TextDisabled("(+%ds queue)", delayQueueSecValue);
            }

            int currentDelay = (mapTypeValue == 0) ? delayFreeplaySecValue
                                                   : (mapTypeValue == 1 ? delayTrainingSecValue : delayWorkshopSecValue);
            if (currentDelay > 0) {
                ImGui::SameLine(0, 10);
                ImGui::TextColored(dim, "|");
                ImGui::SameLine(0, 10);
                ImGui::TextDisabled("Map Delay: +%ds", currentDelay);
            }
        }
        ImGui::EndChildFrame();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1) Global Controls
    // ...
    ImGui::Spacing();

    // 1) Global Controls (Enable/Disable + Map Mode) - above the tabs
    RenderGeneralTab(enabledValue, mapTypeValue);
    ImGui::Spacing();
    ImGui::Separator();

    // Main tab bar - disabled when plugin is off
    if (!enabledValue) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::BeginTabBar("SuiteSpotTabs", ImGuiTabBarFlags_None)) {

        // ===== MAP SELECT TAB =====
        if (ImGui::BeginTabItem(ICON_FA_MAP " Map Select")) {
            if (enabledValue) {
                ImGui::Spacing();

                // Compact single row: [Auto-Queue]  Queue Delay: [__] s  Map Delay: [__] s
                UI::Helpers::CheckboxWithCVar("Auto-Queue", autoQueueValue, "suitespot_auto_queue",
                                              plugin_->cvarManager, plugin_->gameWrapper,
                                              "Automatically queue into the next match after the current match ends.");
                ImGui::SameLine(0, 20);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Queue Delay:");
                ImGui::SameLine(0, 6);
                ImGui::PushButtonRepeat(true);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                UI::Helpers::InputIntWithRange("##QueueDelay", delayQueueSecValue,
                                               UI::SettingsUI::DELAY_QUEUE_MIN_SECONDS,
                                               UI::SettingsUI::DELAY_QUEUE_MAX_SECONDS, 75.0f,
                                               "suitespot_delay_queue_sec", plugin_->cvarManager, plugin_->gameWrapper,
                                               "Wait before auto-queuing.", nullptr);
                ImGui::PopStyleVar();
                ImGui::PopButtonRepeat();
                ImGui::SameLine(0, 3);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("s");
                ImGui::SameLine(0, 16);

                // Map Delay (context-sensitive CVar based on current map type)
                int* currentMapDelayValue = &delayFreeplaySecValue;
                const char* currentMapDelayCVar = "suitespot_delay_freeplay_sec";
                const char* mapDelayTooltip = "Wait before loading Freeplay.";
                if (mapTypeValue == 1) {
                    currentMapDelayValue = &delayTrainingSecValue;
                    currentMapDelayCVar = "suitespot_delay_training_sec";
                    mapDelayTooltip = "Wait before loading Training.";
                } else if (mapTypeValue == 2) {
                    currentMapDelayValue = &delayWorkshopSecValue;
                    currentMapDelayCVar = "suitespot_delay_workshop_sec";
                    mapDelayTooltip = "Wait before loading Workshop.";
                }

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Map Delay:");
                ImGui::SameLine(0, 6);
                ImGui::PushButtonRepeat(true);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                UI::Helpers::InputIntWithRange("##MapDelay", *currentMapDelayValue, 0, 300, 75.0f, currentMapDelayCVar,
                                               plugin_->cvarManager, plugin_->gameWrapper, mapDelayTooltip, nullptr);
                ImGui::PopStyleVar();
                ImGui::PopButtonRepeat();
                ImGui::SameLine(0, 3);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("s");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 2) Map Selection Logic
                RenderMapSelectionTab(mapTypeValue, currentFreeplayCode, currentTrainingCode, currentWorkshopPath,
                                      delayFreeplaySecValue, delayTrainingSecValue, delayWorkshopSecValue,
                                      delayQueueSecValue);
            } else {
                ImGui::Spacing();
                ImGui::TextDisabled("Enable SuiteSpot to configure map settings.");
            }

            ImGui::EndTabItem();
        }

        // ===== LOADOUT MANAGEMENT TAB =====
        if (ImGui::BeginTabItem(ICON_FA_LIST " Loadout")) {
            if (enabledValue) {
                if (plugin_->loadoutUI) {
                    plugin_->loadoutUI->RenderLoadoutControls();
                }
            } else {
                ImGui::Spacing();
                ImGui::TextDisabled("Enable SuiteSpot to manage loadouts.");
            }
            ImGui::EndTabItem();
        }

        // ===== HOTKEYS TAB =====
        if (ImGui::BeginTabItem(ICON_FA_KEYBOARD " Hotkeys")) {
            ImGui::Spacing();
            DrawSectionHeader("Keyboard Shortcuts", ImGui::ColorConvertFloat4ToU32(UI::SECTION_HEADER_COLOR));
            ImGui::Spacing();
            ImGui::TextDisabled("Click " ICON_FA_CIRCLE " to capture a key press, or type the UE3 name manually.");
            ImGui::TextDisabled("Key 1 = trigger.  Key 2 = required held modifier.  Both must be set.");
            ImGui::TextDisabled("Xbox buttons are captured automatically.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Columns(2, "HotkeysTabCols", false);
            ImGui::SetColumnWidth(0, UI::SettingsUI::HOTKEY_LABEL_COL_WIDTH);

            for (int i = 0; i < 5; i++) {
                const auto& row = UI::SettingsUI::HOTKEY_ROWS[i];
                ImGui::PushID(i);

                // Col 0: action label
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(row.label);
                ImGui::NextColumn();

                // Helper: render one key slot (key1 or key2)
                auto renderSlot = [&](int slot, const char* cvarName, const char* inputId, const char* capBtnId,
                                      const char* clrBtnId, const char* tip) {
                    char buf[64] = {};
                    if (auto cvar = plugin_->cvarManager->getCvar(cvarName); !cvar.IsNull())
                        strncpy_s(buf, cvar.getStringValue().c_str(), sizeof(buf) - 1);

                    bool cap = (plugin_->captureRow == i && plugin_->captureSlot == slot);
                    if (cap) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                        ImGui::Button("Press any key...", ImVec2(UI::SettingsUI::HOTKEY_KEY_INPUT_WIDTH, 0));
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::SetNextItemWidth(UI::SettingsUI::HOTKEY_KEY_INPUT_WIDTH);
                        if (ImGui::InputText(inputId, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
                            UI::Helpers::SetCVarSafely(cvarName, std::string(buf), plugin_->cvarManager,
                                                       plugin_->gameWrapper);
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            UI::Helpers::SetCVarSafely(cvarName, std::string(buf), plugin_->cvarManager,
                                                       plugin_->gameWrapper);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
                    }
                    ImGui::SameLine(0, 2);
                    ImGui::PushID(slot);                                           // Unique ID per slot within the row
                    if (ImGui::SmallButton(cap ? ICON_FA_STOP : ICON_FA_CIRCLE)) { // stop / record
                        plugin_->captureRow = cap ? -1 : i;
                        plugin_->captureSlot = slot;
                    }
                    ImGui::PopID();
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetTooltip(cap ? "Cancel (or press Esc)"
                                              : "Click then press any key (Keyboard or Xbox) to capture");
                    }
                    if (!cap && buf[0] != '\0') {
                        ImGui::SameLine(0, 2);
                        if (ImGui::SmallButton(clrBtnId))
                            UI::Helpers::SetCVarSafely(cvarName, std::string(""), plugin_->cvarManager,
                                                       plugin_->gameWrapper);
                    }
                };

                // Col 1: [Key1 slot]  +  [Key2 slot]
                renderSlot(0, row.key1CVar, "##k1", "##cb1", "X##k1x",
                           "Trigger key - click " ICON_FA_CIRCLE " to capture, or type UE3 name");
                ImGui::SameLine(0, 6);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("+");
                ImGui::SameLine(0, 6);
                renderSlot(1, row.key2CVar, "##k2", "##cb2", "X##k2x",
                           "Required held modifier — must be held when Key 1 fires. Both keys must be set.");

                // Validation: key1 set without key2 is invalid (dual-key required)
                {
                    char k1[64] = {}, k2[64] = {};
                    if (auto c = plugin_->cvarManager->getCvar(row.key1CVar); !c.IsNull())
                        strncpy_s(k1, c.getStringValue().c_str(), sizeof(k1) - 1);
                    if (auto c = plugin_->cvarManager->getCvar(row.key2CVar); !c.IsNull())
                        strncpy_s(k2, c.getStringValue().c_str(), sizeof(k2) - 1);
                    if (k1[0] != '\0' && k2[0] == '\0') {
                        ImGui::SameLine(0, 8);
                        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "(!) set Key 2");
                    }
                }

                ImGui::NextColumn();
                ImGui::PopID();
                ImGui::Spacing();
            }
            ImGui::Columns(1);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_SAVE " Save Hotkeys")) plugin_->cvarManager->executeCommand("writeconfig", false);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Persist hotkey settings to config file");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!enabledValue) {
        ImGui::PopStyleVar();
    }

    ImGui::PopFont();
    ImGui::PopStyleColor(25); // all color pushes
    ImGui::PopStyleVar(14);   // all style var pushes
}

void SettingsUI::RenderGeneralTab(bool& enabledValue, int& mapTypeValue)
{
    // Row: [Enable SuiteSpot]  [Fix Training Speed]  ···  Mode: [Freeplay][Training][Workshop]
    UI::Helpers::CheckboxWithCVar("Enable SuiteSpot", enabledValue, "suitespot_enabled", plugin_->cvarManager,
                                  plugin_->gameWrapper, "Enable/disable all SuiteSpot auto-loading and queuing features");
    ImGui::SameLine(0, 20);

    bool gameSpeedFixEnabled = plugin_->settingsSync ? plugin_->settingsSync->IsTrainingGameSpeedFixEnabled() : true;
    if (UI::Helpers::CheckboxWithCVar("Fix Training Speed", gameSpeedFixEnabled, "suitespot_fix_training_gamespeed",
                                      plugin_->cvarManager, plugin_->gameWrapper,
                                      "Sync in-game training speed with sv_soccar_gamespeed in training playlists.")) {
        LOG("Fix Training Game Speed toggled to {}", gameSpeedFixEnabled ? "ON" : "OFF");
    }

    // Right-align map mode CheckButtons
    const char* modeLabels[] = {ICON_FA_GLOBE " Freeplay", ICON_FA_GRADUATION_CAP " Training", ICON_FA_COGS " Workshop"};
    float modeGroupW = ImGui::CalcTextSize("Mode:").x + ImGui::GetStyle().ItemSpacing.x;
    for (int i = 0; i < 3; i++) {
        modeGroupW += ImGui::CalcTextSize(modeLabels[i]).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (i < 2) modeGroupW += 4.0f; // SameLine spacing between buttons
    }
    ImGui::SameLine(ImGui::GetContentRegionMax().x - modeGroupW - ImGui::GetStyle().WindowPadding.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine(0, 4);
        bool active = (mapTypeValue == i);
        bool wasActive = active; // snapshot BEFORE CheckButton modifies it
        if (wasActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, UI::MAP_MODE_ACTIVE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::MAP_MODE_ACTIVE_COLOR);
        }
        if (ImGui::CheckButton(modeLabels[i], &active) && active) {
            mapTypeValue = i;
            LOG("SuiteSpot UI: User switched Map Mode to {}", modeLabels[i]);
            UI::Helpers::SetCVarSafely("suitespot_map_type", mapTypeValue, plugin_->cvarManager, plugin_->gameWrapper);
        }
        if (wasActive) ImGui::PopStyleColor(2); // pop must match push count
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Choose which map type loads after matches:\nFreeplay = Official | Training = Custom Packs | "
                          "Workshop = Modded Maps");
    }
}

void SettingsUI::RenderMapSelectionTab(int mapTypeValue, std::string& currentFreeplayCode,
                                       std::string& currentTrainingCode, std::string& currentWorkshopPath,
                                       int& delayFreeplaySecValue, int& delayTrainingSecValue,
                                       int& delayWorkshopSecValue, int& delayQueueSecValue)
{
    DrawSectionHeader("Map Selection", ImGui::ColorConvertFloat4ToU32(UI::SECTION_HEADER_COLOR));
    ImGui::Spacing();

    if (mapTypeValue == 0) {
        RenderFreeplayMode(currentFreeplayCode);
    } else if (mapTypeValue == 1) {
        RenderTrainingMode(0, currentTrainingCode);
    } else if (mapTypeValue == 2) {
        RenderWorkshopMode(currentWorkshopPath);
    }
}

void SettingsUI::RenderFreeplayMode(std::string& currentFreeplayCode)
{
    // Initialize to first map if empty and maps are available
    if (currentFreeplayCode.empty() && !RLMaps.empty()) {
        currentFreeplayCode = RLMaps[0].code;
        plugin_->settingsSync->SetCurrentFreeplayCode(currentFreeplayCode);
        plugin_->cvarManager->getCvar("suitespot_current_freeplay_code").setValue(currentFreeplayCode);
    }

    // Find current selection index
    int currentIndex = 0;
    for (int i = 0; i < (int)RLMaps.size(); i++) {
        if (RLMaps[i].code == currentFreeplayCode) {
            currentIndex = i;
            break;
        }
    }

    const char* freeplayLabel = RLMaps.empty() ? "<none>" : RLMaps[currentIndex].name.c_str();

    // Build names list for SearchableCombo
    std::vector<std::string> mapNames;
    mapNames.reserve(RLMaps.size());
    for (const auto& m : RLMaps)
        mapNames.push_back(m.name);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Freeplay Map:");
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(UI::SettingsUI::FREEPLAY_MAPS_DROPDOWN_WIDTH);
    if (ImGui::SearchableCombo("##FreeplayMap", &currentIndex, mapNames, freeplayLabel, "Search maps...")) {
        if (currentIndex >= 0 && currentIndex < (int)RLMaps.size()) {
            currentFreeplayCode = RLMaps[currentIndex].code;
            LOG("SuiteSpot UI: User selected Freeplay map: {} ({})", RLMaps[currentIndex].name, currentFreeplayCode);
            plugin_->settingsSync->SetCurrentFreeplayCode(currentFreeplayCode);
            plugin_->cvarManager->getCvar("suitespot_current_freeplay_code").setValue(currentFreeplayCode);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select which stadium loads after matches");
}

void SettingsUI::RenderTrainingMode(int trainingModeValue, std::string& currentTrainingCode)
{
    RenderSinglePackMode(currentTrainingCode);
}

void SettingsUI::RenderWorkshopMode(std::string& currentWorkshopPath)
{
    if (ImGui::BeginTabBar("##WorkshopSubTabs")) {
        if (ImGui::BeginTabItem("Installed Maps")) {
            RenderInstalledMaps(currentWorkshopPath);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Map Browser")) {
            RenderWorkshopBrowserTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void SettingsUI::RenderInstalledMaps(std::string& currentWorkshopPath)
{
    // Header with Refresh button (right-aligned)
    ImGui::Spacing();
    {
        float btnW = ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - btnW);
    }
    if (ImGui::Button("Refresh", ImVec2(0, 0))) {
        plugin_->LoadWorkshopMaps();
        selectedWorkshopIndex = -1; // Reset selection
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rescan workshop folders for maps");
    }
    ImGui::Spacing();

    // Check if we have any maps
    if (RLWorkshop.empty()) {
        ImGui::TextColored(UI::WorkshopBrowserUI::NO_MAPS_COLOR, "No workshop maps found.");
        ImGui::TextDisabled("Maps are discovered from:");
        ImGui::BulletText("WorkshopMapLoader configured path");
        ImGui::BulletText("Epic Games install mods folder");
        ImGui::BulletText("Steam install mods folder");
        ImGui::Spacing();
        ImGui::TextDisabled("Download maps from the Map Browser tab.");
        return;
    }

    // Initialize selection from current CVar if needed
    if (selectedWorkshopIndex < 0 && !currentWorkshopPath.empty()) {
        for (int i = 0; i < (int)RLWorkshop.size(); i++) {
            if (RLWorkshop[i].filePath == currentWorkshopPath) {
                selectedWorkshopIndex = i;
                break;
            }
        }
    }

    // Clamp selection to valid range
    if (selectedWorkshopIndex >= (int)RLWorkshop.size()) {
        selectedWorkshopIndex = (int)RLWorkshop.size() - 1;
    }

    // Calculate panel widths
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::WorkshopBrowserUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::WorkshopBrowserUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    // Two-panel layout
    ImGui::BeginGroup();

    // === LEFT PANEL: Map List ===
    if (ImGui::BeginChild("WorkshopMapList", ImVec2(leftWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        // Filter input
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##WorkshopInstFilter", "Filter maps...", workshopInstalledFilterBuf,
                                 sizeof(workshopInstalledFilterBuf));

        // Build filter string (lowercase for case-insensitive match)
        std::string filterStr = workshopInstalledFilterBuf;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

        // Count visible maps for header
        int visibleCount = 0;
        for (const auto& e : RLWorkshop) {
            if (filterStr.empty()) {
                visibleCount++;
                continue;
            }
            std::string nl = e.name;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            if (nl.find(filterStr) != std::string::npos) visibleCount++;
        }
        ImGui::TextDisabled(filterStr.empty() ? "%d maps" : "%d / %d maps", visibleCount, (int)RLWorkshop.size());
        ImGui::Separator();

        for (int i = 0; i < (int)RLWorkshop.size(); i++) {
            const auto& entry = RLWorkshop[i];

            // Apply filter
            if (!filterStr.empty()) {
                std::string nameLower = entry.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(filterStr) == std::string::npos) continue;
            }

            bool isSelected = (i == selectedWorkshopIndex);
            bool isCurrentAutoLoad = (entry.filePath == currentWorkshopPath);

            ImGui::PushID(i);

            if (isCurrentAutoLoad) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::SELECTED_BADGE_COLOR);
                ImGui::Text(">");
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            if (ImGui::Selectable(entry.name.c_str(), isSelected, ImGuiSelectableFlags_None)) {
                selectedWorkshopIndex = i;
                currentWorkshopPath = entry.filePath;
                LOG("SuiteSpot UI: User selected Workshop map: {} ({})", entry.name, entry.filePath);
                plugin_->settingsSync->SetCurrentWorkshopPath(entry.filePath);
                if (auto cvar = plugin_->cvarManager->getCvar("suitespot_current_workshop_path")) {
                    cvar.setValue(entry.filePath);
                }
            }

            // Double-click to load immediately
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                SuiteSpot* p = plugin_;
                std::string path = entry.filePath;
                p->gameWrapper->SetTimeout(
                    [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                    0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
            }

            // Hand cursor on hover
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##WsCtx")) {
                if (ImGui::MenuItem("Load Now")) {
                    SuiteSpot* p = plugin_;
                    std::string path = entry.filePath;
                    p->gameWrapper->SetTimeout(
                        [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                        0.0f);
                    statusMessage.ShowSuccess("Loading Workshop Map", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                    if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
                }
                if (!isCurrentAutoLoad && ImGui::MenuItem("Select for Post-Match")) {
                    selectedWorkshopIndex = i;
                    currentWorkshopPath = entry.filePath;
                    plugin_->settingsSync->SetCurrentWorkshopPath(entry.filePath);
                    if (auto cvar = plugin_->cvarManager->getCvar("suitespot_current_workshop_path"))
                        cvar.setValue(entry.filePath);
                    statusMessage.ShowSuccess("Workshop map selected", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                ImGui::EndPopup();
            }

            // Scroll to selected on first appearance
            if (isSelected && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT PANEL: Details Pane ===
    if (ImGui::BeginChild("WorkshopMapDetails", ImVec2(rightWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        if (selectedWorkshopIndex >= 0 && selectedWorkshopIndex < (int)RLWorkshop.size()) {
            auto& selectedMap = RLWorkshop[selectedWorkshopIndex];

            // Load preview image if needed (lazy loading)
            if (!selectedMap.previewPath.empty() && !selectedMap.isImageLoaded && !selectedMap.previewImage) {
                selectedMap.previewImage = std::make_shared<ImageWrapper>(selectedMap.previewPath.string(), false, true);
                selectedMap.isImageLoaded = true;
            }

            // Preview image
            if (selectedMap.previewImage && selectedMap.previewImage->GetImGuiTex()) {
                ImGui::Image(selectedMap.previewImage->GetImGuiTex(),
                             ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                    UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            } else {
                // Placeholder box
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(p,
                                        ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                               p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT),
                                        ImColor(40, 40, 45, 255), 4.0f);
                drawList->AddText(ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH / 2 - 40,
                                         p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT / 2 - 8),
                                  ImColor(100, 100, 100, 255), "No Preview");
                ImGui::Dummy(
                    ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH, UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            }

            ImGui::Spacing();

            // Map name (large)
            ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::MAP_NAME_COLOR);
            ImGui::TextWrapped("%s", selectedMap.name.c_str());
            ImGui::PopStyleColor();

            // Author
            if (!selectedMap.author.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::AUTHOR_COLOR);
                ImGui::Text("By: %s", selectedMap.author.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            // Description
            if (!selectedMap.description.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::DESCRIPTION_COLOR);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", selectedMap.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Current selection indicator
            bool isCurrentAutoLoad = (selectedMap.filePath == currentWorkshopPath);
            if (isCurrentAutoLoad) {
                ImGui::TextColored(UI::WorkshopBrowserUI::SELECTED_BADGE_COLOR, "Selected for Post-Match Auto-Load");
                ImGui::Spacing();
            }

            // Action buttons
            if (!isCurrentAutoLoad) {
                if (ImGui::Button("Select for Post-Match", ImVec2(0, 26))) {
                    plugin_->settingsSync->SetCurrentWorkshopPath(selectedMap.filePath);
                    plugin_->cvarManager->getCvar("suitespot_current_workshop_path").setValue(selectedMap.filePath);
                    currentWorkshopPath = selectedMap.filePath;
                    statusMessage.ShowSuccess("Workshop map selected", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Set this map to load after matches end");
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Load Now", ImVec2(0, 26))) {
                SuiteSpot* p = plugin_;
                std::string path = selectedMap.filePath;
                p->gameWrapper->SetTimeout(
                    [p, path](GameWrapper* gw) { p->cvarManager->executeCommand("load_workshop \"" + path + "\""); },
                    0.0f);
                statusMessage.ShowSuccess("Loading Workshop Map", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) {
                    p->cvarManager->executeCommand("togglemenu settings");
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Load this workshop map immediately");
            }

        } else {
            // No selection
            ImGui::TextDisabled("Select a map from the list");
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

void SettingsUI::RenderSinglePackMode(std::string& currentTrainingCode)
{
    // Toggle between Flicks Picks and Your Favorites
    int listType = plugin_->settingsSync->GetQuickPicksListType();

    ImGui::TextUnformatted("List Type:");
    ImGui::SameLine();

    if (ImGui::RadioButton("Flicks Picks", listType == 0)) {
        UI::Helpers::SetCVarSafely("suitespot_quickpicks_list_type", 0, plugin_->cvarManager, plugin_->gameWrapper);
        selectedQuickPickIndex = -1;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Curated selection of 10 essential training packs");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Quick Picks", listType == 1)) {
        UI::Helpers::SetCVarSafely("suitespot_quickpicks_list_type", 1, plugin_->cvarManager, plugin_->gameWrapper);
        selectedQuickPickIndex = -1;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Your most-used training packs based on load history");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_SEARCH " Open Pack Browser", ImVec2(0, 0))) {
        SuiteSpot* p = plugin_;
        p->gameWrapper
            ->SetTimeout([p](GameWrapper* gw) { p->cvarManager->executeCommand("togglemenu suitespot_browser"); }, 0.0f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open the full training pack browser to manage bags and packs");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Build pack metadata list for this render pass
    std::vector<std::string> quickPicks = GetQuickPicksList();
    std::string selectedCode = plugin_->settingsSync->GetQuickPicksSelectedCode();

    // Default selection on first render
    if (selectedCode.empty() && !quickPicks.empty()) {
        selectedCode = quickPicks[0];
        plugin_->settingsSync->SetQuickPicksSelected(selectedCode);
        plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(selectedCode);
    }

    // Clamp index
    if (selectedQuickPickIndex >= (int)quickPicks.size()) {
        selectedQuickPickIndex = -1;
    }

    // Sync selectedQuickPickIndex from selectedCode on first render (fixes empty right panel)
    if (selectedQuickPickIndex < 0 && !selectedCode.empty()) {
        for (int i = 0; i < (int)quickPicks.size(); i++) {
            if (quickPicks[i] == selectedCode) {
                selectedQuickPickIndex = i;
                break;
            }
        }
    }

    // Helper: resolve TrainingEntry* by code
    auto resolveEntry = [&](const std::string& code) -> const TrainingEntry* {
        const auto& packs = plugin_->trainingPackMgr ? plugin_->trainingPackMgr->GetPacks() : RLTraining;
        auto it = std::find_if(packs.begin(), packs.end(), [&](const TrainingEntry& e) { return e.code == code; });
        if (it != packs.end()) return &(*it);
        return nullptr;
    };

    // Calculate panel widths
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::QuickPicksUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::QuickPicksUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup();

    // === LEFT PANEL: Pack List ===
    if (ImGui::BeginChild("QuickPicksList", ImVec2(leftWidth, UI::QuickPicksUI::BROWSER_HEIGHT), true)) {
        const char* header = (listType == 0) ? "Flicks Picks" : "Quick Picks";
        ImGui::TextDisabled("%s  (%d packs)", header, (int)quickPicks.size());
        ImGui::Separator();

        for (int i = 0; i < (int)quickPicks.size(); i++) {
            const std::string& code = quickPicks[i];
            bool isPostMatch = (code == selectedCode);
            bool isHighlighted = (i == selectedQuickPickIndex);

            ImGui::PushID(i);

            // Post-match selection marker
            if (isPostMatch) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::SELECTED_BADGE_COLOR);
                ImGui::TextUnformatted(">");
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            // Resolve display name
            std::string displayName = code;
            if (const TrainingEntry* e = resolveEntry(code)) {
                displayName = e->name;
            } else {
                for (const auto& d : DefaultPacks::FLICKS_PICKS) {
                    if (d.code == code) {
                        displayName = d.name;
                        break;
                    }
                }
            }

            if (ImGui::Selectable(displayName.c_str(), isHighlighted)) {
                selectedQuickPickIndex = i;
            }

            // Double-click to load immediately
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                SuiteSpot* p = plugin_;
                std::string c = code;
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(c);
                p->gameWrapper->SetTimeout([p, c](GameWrapper*) { p->cvarManager->executeCommand("load_training " + c); },
                                           0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
            }

            // Hand cursor on hover
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##QpCtx")) {
                if (ImGui::MenuItem("Load Now")) {
                    SuiteSpot* p = plugin_;
                    std::string c = code;
                    if (p->usageTracker) p->usageTracker->IncrementLoadCount(c);
                    p->gameWrapper
                        ->SetTimeout([p, c](GameWrapper*) { p->cvarManager->executeCommand("load_training " + c); },
                                     0.0f);
                    statusMessage.ShowSuccess("Loading Training Pack", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                    if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
                }
                if (!isPostMatch && ImGui::MenuItem("Select for Post-Match")) {
                    selectedCode = code;
                    plugin_->settingsSync->SetQuickPicksSelected(code);
                    plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(code);
                    statusMessage.ShowSuccess("Pack selected for post-match", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::MenuItem("Copy Code")) {
                    ImGui::SetClipboardText(code.c_str());
                    statusMessage.ShowSuccess("Code copied!", 1.5f, UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                ImGui::EndPopup();
            }

            // Scroll to selected item on first appear
            if (isHighlighted && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT PANEL: Details ===
    if (ImGui::BeginChild("QuickPicksDetails", ImVec2(rightWidth, UI::QuickPicksUI::BROWSER_HEIGHT), true)) {
        if (selectedQuickPickIndex >= 0 && selectedQuickPickIndex < (int)quickPicks.size()) {
            const std::string& code = quickPicks[selectedQuickPickIndex];
            bool isPostMatch = (code == selectedCode);

            // Resolve metadata — try live cache first, then DefaultPacks
            std::string name = code;
            std::string creator;
            std::string difficulty;
            std::string description;
            std::vector<std::string> tags;
            int shots = 0;

            if (const TrainingEntry* e = resolveEntry(code)) {
                name = e->name;
                creator = e->creator;
                difficulty = e->difficulty;
                shots = e->shotCount;
                description = e->staffComments.empty() ? e->notes : e->staffComments;
                tags = e->tags;
            } else {
                for (const auto& d : DefaultPacks::FLICKS_PICKS) {
                    if (d.code == code) {
                        name = d.name;
                        shots = d.shotCount;
                        description = d.description;
                        break;
                    }
                }
            }

            // Pack name
            ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::PACK_NAME_COLOR);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopStyleColor();

            // Pack code (copyable hint)
            ImGui::TextDisabled("Code: %s", code.c_str());

            // Creator
            if (!creator.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::CREATOR_COLOR);
                ImGui::Text("By: %s", creator.c_str());
                ImGui::PopStyleColor();
            }

            // Shots + difficulty on same line
            if (shots > 0) ImGui::Text("%d shots", shots);
            if (!difficulty.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("| %s", difficulty.c_str());
            }

            // Tags
            if (!tags.empty()) {
                ImGui::Spacing();
                for (const auto& tag : tags) {
                    ImGui::TextDisabled("[%s]", tag.c_str());
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

            // Description
            if (!description.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, UI::QuickPicksUI::DESCRIPTION_COLOR);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Post-match selection indicator
            if (isPostMatch) {
                ImGui::TextColored(UI::QuickPicksUI::SELECTED_BADGE_COLOR, "Selected for Post-Match Auto-Load");
                ImGui::Spacing();
            }

            // Action buttons
            if (!isPostMatch) {
                if (ImGui::Button("Select for Post-Match", ImVec2(0, 26))) {
                    selectedCode = code;
                    plugin_->settingsSync->SetQuickPicksSelected(code);
                    plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(code);
                    statusMessage.ShowSuccess("Pack selected for post-match", 2.0f,
                                              UI::StatusMessage::DisplayMode::TimerWithFade);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Load this pack automatically after matches end");
                }
                ImGui::SameLine();
            }

            if (ImGui::Button("Load Now", ImVec2(0, 26))) {
                SuiteSpot* p = plugin_;
                std::string c = code;
                if (p->usageTracker) p->usageTracker->IncrementLoadCount(c);
                p->gameWrapper->SetTimeout([p, c](GameWrapper*) { p->cvarManager->executeCommand("load_training " + c); },
                                           0.0f);
                statusMessage.ShowSuccess("Loading Training Pack", 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                if (p->cvarManager) p->cvarManager->executeCommand("togglemenu settings");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Load this training pack immediately");
            }

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_COPY " Copy Code", ImVec2(0, 26))) {
                ImGui::SetClipboardText(code.c_str());
                statusMessage.ShowSuccess("Code copied!", 1.5f, UI::StatusMessage::DisplayMode::TimerWithFade);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy pack code: %s", code.c_str());

        } else {
            ImGui::TextDisabled("Select a pack from the list");
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

std::vector<std::string> SettingsUI::GetQuickPicksList()
{
    int listType = plugin_->settingsSync->GetQuickPicksListType();

    // Build Flicks Picks list
    std::vector<std::string> flicksPicks;
    for (const auto& p : DefaultPacks::FLICKS_PICKS) {
        flicksPicks.push_back(p.code);
    }

    // If Flicks Picks mode is selected, always return Flicks Picks
    if (listType == 0) {
        return flicksPicks;
    }

    // Your Quick Picks mode - use usage tracker
    if (!plugin_->usageTracker) return flicksPicks;

    // If first run or no data, fallback to Flicks Picks
    if (plugin_->usageTracker->IsFirstRun()) {
        return flicksPicks;
    }

    int count = plugin_->settingsSync->GetQuickPicksCount();
    auto topCodes = plugin_->usageTracker->GetTopUsedCodes(count);

    // If no favorites yet, fallback to Flicks Picks
    if (topCodes.empty()) return flicksPicks;

    return topCodes;
}

// ── Workshop Search Scoring ────────────────────────────────────────────────
// Returns relevance score >= 1 if query found in result, -1 if no match.
// Higher = better match. Uses search-engine best practices:
//   1000 = exact name match
//    900 = name starts with query (prefix)
//    800 = any word in name starts with query (word boundary)
//    700-N = query found at position N in name (earlier = better)
//    +50 per extra occurrence (frequency bonus)
//    +100 if author also matches
static int ScoreResult(const std::string& queryLower, const WorkshopMap& result)
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
    if (!authorLower.empty() && authorLower.find(queryLower) != std::string::npos) score += 100;

    return std::max(score, 1);
}

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
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    // Build display list
    displayResultList.clear();
    displayResultList.reserve(scored.size());
    for (auto& [score, idx] : scored)
        displayResultList.push_back(cachedResultList[idx]);

    // Restore selection by ID so clicking a map survives list updates
    selectedBrowserIndex = -1;
    if (!selectedMapID.empty()) {
        for (int i = 0; i < (int)displayResultList.size(); ++i) {
            if (displayResultList[i].ID == selectedMapID) {
                selectedBrowserIndex = i;
                break;
            }
        }
    }
}

void SettingsUI::RenderWorkshopBrowserTab()
{
    if (!plugin_->workshopDownloader) {
        ImGui::TextDisabled("Workshop downloader not initialized");
        return;
    }

    ImGui::Spacing();

    static bool pathInit = false;
    if (!pathInit) {
        std::string defaultPath;
        if (plugin_->mapManager) {
            auto resolved = plugin_->mapManager->ResolveConfiguredWorkshopRoot();
            if (!resolved.empty() && fs::exists(resolved)) defaultPath = resolved.string();
        }
        if (defaultPath.empty()) defaultPath = plugin_->gameWrapper->GetDataFolder().string() + "\\SuiteSpot\\Workshop";
        strncpy_s(workshopDownloadPathBuf, defaultPath.c_str(), sizeof(workshopDownloadPathBuf) - 1);
        pathInit = true;
    }

    // Auto-fetch all maps on first open
    static bool mapsLoaded = false;
    if (!mapsLoaded && !plugin_->workshopDownloader->isSearching) {
        plugin_->workshopDownloader->GetResults("");
        mapsLoaded = true;
    }

    // Local search bar — filters the cached full map list in real time
    {
        float clearBtnW = strlen(localFilterBuf) > 0
                              ? ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f +
                                    ImGui::GetStyle().ItemSpacing.x
                              : 0.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clearBtnW);
    }
    ImGui::InputTextWithHint("##WorkshopSearch", "Search workshop maps...", localFilterBuf, IM_ARRAYSIZE(localFilterBuf));
    if (strlen(localFilterBuf) > 0) {
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            memset(localFilterBuf, 0, sizeof(localFilterBuf));
            lastLocalFilter.clear();
        }
    }

    // Status line
    if (plugin_->workshopDownloader->isSearching) {
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            plugin_->workshopDownloader->StopSearch();
            selectedMapID.clear();
            selectedBrowserIndex = -1;
            mapsLoaded = false;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Loading...");
    } else if (!cachedResultList.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%d / %d maps", (int)displayResultList.size(), (int)cachedResultList.size());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Download progress bar
    if (plugin_->workshopDownloader->isDownloading) {
        float downloaded = static_cast<float>(plugin_->workshopDownloader->downloadedBytes.load());
        float fileSize = static_cast<float>(plugin_->workshopDownloader->downloadFileSize.load());
        float fraction = (fileSize > 0) ? (downloaded / fileSize) : 0.0f;

        char label[64];
        snprintf(label, sizeof(label), "%.1f / %.1f MB", downloaded / 1048576.0f, fileSize / 1048576.0f);
        ImGui::ProgressBar(fraction, ImVec2(-1, 20), label);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Search results
    RenderWorkshopResults(workshopDownloadPathBuf);

    // Popups - donor pattern: thread sets flag, render loop opens popup
    if (plugin_->workshopDownloader->UserIsChoosingYESorNO) {
        ImGui::OpenPopup("Download?");
    }
    RenderAcceptDownload();
    RenderInfoPopup("Downloading?",
                    "A download is already running!\\nYou cannot download 2 workshops at the same time.");
    RenderInfoPopup("Exists?", "This directory is not valid!");

    if (plugin_->workshopDownloader->FolderErrorBool) {
        RenderInfoPopup("FolderError", plugin_->workshopDownloader->FolderErrorText.c_str());
    }
}

void SettingsUI::RenderWorkshopResults(const char* mapspath)
{
    if (!plugin_->workshopDownloader) return;

    int currentVersion = plugin_->workshopDownloader->listVersion.load();
    if (currentVersion != lastListVersion) {
        std::lock_guard<std::mutex> lock(plugin_->workshopDownloader->resultsMutex);
        auto& fullList = plugin_->workshopDownloader->mapResults;
        cachedResultList.clear();
        for (auto& result : fullList) {
            // Filter out maps that already exist in the download directory
            std::string safeName = plugin_->workshopDownloader->SanitizeMapName(result.Name);
            std::string mapDir = std::string(mapspath);
            if (!mapDir.empty() && (mapDir.back() != '/' && mapDir.back() != '\\')) mapDir += "/";
            mapDir += safeName;
            if (!fs::exists(mapDir)) {
                cachedResultList.push_back(result);
            }
        }
        LOG("UI Synced list. New version: {}, items: {} (filtered from {})", currentVersion, cachedResultList.size(),
            fullList.size());
        lastListVersion = currentVersion;
        RebuildDisplayList(); // Score and sort the freshly-built cache
    }

    // Re-rank if user changed the local filter text
    std::string currentFilter = localFilterBuf;
    if (currentFilter != lastLocalFilter) {
        lastLocalFilter = currentFilter;
        RebuildDisplayList();
    }

    if (displayResultList.empty()) return;

    // Clamp selection
    if (selectedBrowserIndex >= (int)displayResultList.size()) selectedBrowserIndex = (int)displayResultList.size() - 1;

    // Two-panel layout matching Installed Maps design
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::WorkshopBrowserUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::WorkshopBrowserUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup();

    // === LEFT PANEL: Map List ===
    if (ImGui::BeginChild("##BrowserMapList", ImVec2(leftWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        ImGui::TextDisabled("%d maps", (int)displayResultList.size());
        ImGui::Separator();

        for (int i = 0; i < (int)displayResultList.size(); i++) {
            auto& mapResult = displayResultList[i];
            bool isSelected = (i == selectedBrowserIndex);
            bool hasReleases = !mapResult.releases.empty();

            ImGui::PushID(i);

            // Show loading indicator for maps still fetching releases
            if (!hasReleases) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            }

            if (ImGui::Selectable(mapResult.Name.c_str(), isSelected)) {
                selectedBrowserIndex = i;
                selectedMapID = mapResult.ID;
            }

            if (!hasReleases) {
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === RIGHT PANEL: Details + Download ===
    if (ImGui::BeginChild("##BrowserMapDetails", ImVec2(rightWidth, UI::WorkshopBrowserUI::BROWSER_HEIGHT), true)) {
        if (selectedBrowserIndex >= 0 && selectedBrowserIndex < (int)displayResultList.size()) {
            auto& mapResult = displayResultList[selectedBrowserIndex];

            // Preview image
            std::shared_ptr<ImageWrapper> image = nullptr;
            if (!mapResult.ID.empty()) {
                auto it = workshopImageCache.find(mapResult.ID);
                if (it != workshopImageCache.end()) {
                    image = it->second;
                } else if (!mapResult.ImagePath.empty() && mapResult.isImageLoaded) {
                    try {
                        image = std::make_shared<ImageWrapper>(mapResult.ImagePath.string(), false, true);
                        if (workshopImageCache.size() >= 150) workshopImageCache.erase(workshopImageCache.begin());
                        workshopImageCache[mapResult.ID] = image;
                    } catch (...) {}
                }
            }

            if (image && image->GetImGuiTex()) {
                ImGui::Image(image->GetImGuiTex(), ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                                          UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            } else {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p,
                                  ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH,
                                         p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT),
                                  ImColor(40, 40, 45, 255), 4.0f);
                dl->AddText(ImVec2(p.x + UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH / 2 - 40,
                                   p.y + UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT / 2 - 8),
                            ImColor(100, 100, 100, 255), "No Preview");
                ImGui::Dummy(
                    ImVec2(UI::WorkshopBrowserUI::PREVIEW_IMAGE_WIDTH, UI::WorkshopBrowserUI::PREVIEW_IMAGE_HEIGHT));
            }

            ImGui::Spacing();

            // Map name
            ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::MAP_NAME_COLOR);
            ImGui::TextWrapped("%s", mapResult.Name.c_str());
            ImGui::PopStyleColor();

            // Author
            if (!mapResult.Author.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::AUTHOR_COLOR);
                ImGui::Text("By: %s", mapResult.Author.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            // Description
            if (!mapResult.Description.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, UI::WorkshopBrowserUI::DESCRIPTION_COLOR);
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextWrapped("%s", mapResult.Description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Download button
            bool hasReleases = !mapResult.releases.empty();
            if (hasReleases) {
                if (ImGui::Button(ICON_FA_DOWNLOAD " Download", ImVec2(0, 26))) {
                    if (!plugin_->workshopDownloader->isDownloading && fs::exists(mapspath)) {
                        ImGui::OpenPopup("Releases");
                    } else if (!fs::exists(mapspath)) {
                        ImGui::OpenPopup("Exists?");
                    } else {
                        ImGui::OpenPopup("Downloading?");
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Download this map to your workshop folder");
                }
                RenderReleases(mapResult, mapspath);
            } else {
                ImGui::TextDisabled("Loading releases...");
            }
        } else {
            ImGui::TextDisabled("Select a map from the list");
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

void SettingsUI::RenderReleases(WorkshopMap map, const char* mapspath)
{
    if (ImGui::BeginPopupModal("Releases", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (int releasesIndex = 0; releasesIndex < (int)map.releases.size(); releasesIndex++) {
            WorkshopRelease release = map.releases[releasesIndex];

            if (ImGui::Button(release.tag_name.c_str(), ImVec2(0, 20))) {
                if (!plugin_->workshopDownloader->isDownloading && fs::exists(mapspath)) {
                    auto downloader = plugin_->workshopDownloader;
                    std::string path = std::string(mapspath);
                    std::thread t2([downloader, path, map, release]() { downloader->DownloadMap(path, map, release); });
                    t2.detach();
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        if (ImGui::Button("Cancel", ImVec2(0, 20))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SettingsUI::RenderAcceptDownload()
{
    if (!plugin_->workshopDownloader) return;

    RenderYesNoPopup(
        "Download?", "Do you really want to download?\nYou'll not be able to cancel if you start it.",
        [this]() {
            // User confirmed - signal the waiting thread
            plugin_->workshopDownloader->AcceptTheDownload = true;
            plugin_->workshopDownloader->UserIsChoosingYESorNO = false;
            ImGui::CloseCurrentPopup();
        },
        [this]() {
            // User cancelled - signal the waiting thread
            plugin_->workshopDownloader->AcceptTheDownload = false;
            plugin_->workshopDownloader->UserIsChoosingYESorNO = false;
            ImGui::CloseCurrentPopup();
        });
}

void SettingsUI::RenderYesNoPopup(const char* popupName, const char* label, std::function<void()> yesFunc,
                                  std::function<void()> noFunc)
{
    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", label);
        ImGui::NewLine();

        CenterNextItem(0.0f);
        ImGui::BeginGroup();
        {
            if (ImGui::Button("YES", ImVec2(0, 25.0f))) {
                try {
                    yesFunc();
                } catch (const std::exception& ex) {
                    LOG("Popup error: {}", ex.what());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("NO", ImVec2(0, 25.0f))) {
                noFunc();
            }
            ImGui::EndGroup();
        }

        ImGui::EndPopup();
    }
}

void SettingsUI::RenderInfoPopup(const char* popupName, const char* label)
{
    if (ImGui::BeginPopupModal(popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", label);
        ImGui::NewLine();
        CenterNextItem(0.0f);
        if (ImGui::Button("OK", ImVec2(0, 25.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SettingsUI::CenterNextItem(float itemWidth)
{
    auto windowWidth = ImGui::GetWindowSize().x;
    ImGui::SetCursorPosX((windowWidth - itemWidth) * 0.5f);
}

std::string SettingsUI::LimitTextSize(std::string str, float maxTextSize)
{
    while (ImGui::CalcTextSize(str.c_str()).x > maxTextSize) {
        if (str.empty()) break;
        str = str.substr(0, str.size() - 1);
    }
    return str;
}
void SettingsUI::RenderTextureCheck()
{
    if (!plugin_->textureDownloader) return;

    if (ImGui::Button("Check Textures")) {
        showTexturePopup = true;
        ImGui::OpenPopup("DownloadTextures");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Check for missing workshop textures and install them");
    }

    if (showTexturePopup) {
        std::vector<std::string> missing = plugin_->textureDownloader->CheckMissingTextures();
        RenderDownloadTexturesPopup(missing);
    }
}

void SettingsUI::RenderDownloadTexturesPopup(const std::vector<std::string>& missingFiles)
{
    if (ImGui::BeginPopupModal("DownloadTextures", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!missingFiles.empty()) {
            ImGui::Text("It seems like the workshop textures aren't installed.");
            ImGui::Text("You can still play without them but some maps will have white/weird textures.");

            if (plugin_->textureDownloader->isDownloading) {
                ImGui::Separator();
                ImGui::Text("Downloading... %d%%", plugin_->textureDownloader->downloadProgress.load());
                ImGui::ProgressBar(plugin_->textureDownloader->downloadProgress.load() / 100.0f, ImVec2(300, 20));
                ImGui::Separator();
            }

            ImGui::NewLine();

            if (ImGui::BeginChild("##MissingFiles", ImVec2(300, 150), true)) {
                ImGui::Text("Missing Files (%d):", (int)missingFiles.size());
                ImGui::Separator();
                for (const auto& file : missingFiles) {
                    ImGui::Text("%s", file.c_str());
                }
                ImGui::EndChild();
            }

            ImGui::NewLine();

            if (ImGui::Button("Download & Install", ImVec2(0, 25)) && !plugin_->textureDownloader->isDownloading) {
                plugin_->textureDownloader->DownloadAndInstallTextures();
            }

            ImGui::SameLine();

            if (ImGui::Button("Close", ImVec2(0, 25))) {
                showTexturePopup = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Workshop textures are installed!");
            ImGui::NewLine();
            if (ImGui::Button("OK", ImVec2(0, 25))) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}
```

## File: src/ui/SettingsUI.h
```c
#pragma once
#include "IMGUI/imgui.h"
#include "StatusMessageUI.h"
#include "WorkshopDownloader.h"
#include <string>
#include <vector>
#include <map>

/*
 * ======================================================================================
 * SETTINGS UI: THE CONFIGURATION MENU
 * ======================================================================================
 * 
 * WHAT IS THIS?
 * This class builds the visual menu you see when you press F2 in Rocket League and
 * click on the "SuiteSpot" tab.
 * 
 * WHY IS IT HERE?
 * Users need a way to change settings (like delay times or which map to load).
 * This class translates those settings into buttons, sliders, and checkboxes.
 * 
 * HOW DOES IT WORK?
 * 1. `RenderMainSettingsWindow()`: This is the main loop. It draws the tabs ("Main Settings",
 *    "Auto-Queue", etc.).
 * 2. It reads the current values from `SuiteSpot` (which gets them from `SettingsSync`).
 * 3. When you change a value (like sliding a slider), it immediately tells `SettingsSync`
 *    to save that new value to the config file.
 * 4. It uses `UI::Helpers` to make the code cleaner (so we don't have to write 
 *    "Draw Slider" logic 50 times).
 */

class SuiteSpot;

class SettingsUI
{
  public:
    explicit SettingsUI(SuiteSpot* plugin);
    void RenderMainSettingsWindow();

  private:
    SuiteSpot* plugin_;
    UI::StatusMessage statusMessage; // Shows "Success!" or error messages

    // Internal helpers to draw specific tabs
    // These break the big menu into smaller, manageable chunks
    void RenderGeneralTab(bool& enabledValue, int& mapTypeValue);
    void RenderMapSelectionTab(int mapTypeValue, std::string& currentFreeplayCode, std::string& currentTrainingCode,
                               std::string& currentWorkshopPath, int& delayFreeplaySecValue, int& delayTrainingSecValue,
                               int& delayWorkshopSecValue, int& delayQueueSecValue);
    void RenderFreeplayMode(std::string& currentFreeplayCode);
    void RenderTrainingMode(int trainingModeValue, std::string& currentTrainingCode);
    void RenderWorkshopMode(std::string& currentWorkshopPath);
    void RenderInstalledMaps(std::string& currentWorkshopPath);

    void RenderSinglePackMode(std::string& currentTrainingCode);
    std::vector<std::string> GetQuickPicksList();

    // Workshop browser tab
    void RenderWorkshopBrowserTab();
    void RebuildDisplayList(); // Re-scores and sorts displayResultList from cachedResultList
    void RenderTextureCheck();
    void RenderDownloadTexturesPopup(const std::vector<std::string>& missingFiles);

    void RenderWorkshopResults(const char* mapspath);
    void RenderReleases(WorkshopMap map, const char* mapspath);
    void RenderAcceptDownload();
    void RenderYesNoPopup(const char* popupName, const char* label, std::function<void()> yesFunc,
                          std::function<void()> noFunc);
    void RenderInfoPopup(const char* popupName, const char* label);
    void CenterNextItem(float itemWidth);
    std::string LimitTextSize(std::string str, float maxTextSize);

    // Workshop path configuration state
    char workshopPathBuf[512] = {0};
    bool workshopPathInit = false;
    std::string workshopPathCache = "";

    // Workshop browser state
    char workshopSearchBuf[256] = {0}; // kept for legacy/unused
    char workshopDownloadPathBuf[512] = {0};

    // Workshop local browser state (two-panel layout)
    int selectedWorkshopIndex = -1;       // Currently selected in list
    std::string lastSelectedWorkshopPath; // Track path to detect changes

    // Quick Picks two-panel layout state
    int selectedQuickPickIndex = -1;       // Currently highlighted in left panel
    bool scrollToSelectedQuickPick = true; // Scroll to selection on first appear

    // Workshop installed maps filter
    char workshopInstalledFilterBuf[128] = {0};
    bool scrollToSelectedWorkshop = true; // Scroll to selection on first appear

    // Workshop browser state
    int selectedBrowserIndex = -1;
    std::string selectedMapID; // preserves selection across list rebuilds
    std::vector<WorkshopMap> cachedResultList;
    std::vector<WorkshopMap> displayResultList;
    int lastListVersion = -1;       // Track version to know when to refresh cache
    char localFilterBuf[256] = {0}; // Real-time local filter (re-ranks without API call)
    std::string lastLocalFilter;    // Detect when filter text changes

    // Image cache - persists across list refreshes, keyed by map ID
    std::map<std::string, std::shared_ptr<ImageWrapper>> workshopImageCache;

    // Texture popup state
    bool showTexturePopup = false;
};
```

## File: src/ui/StatusMessageUI.cpp
```cpp
#include "pch.h"
#include "StatusMessageUI.h"

namespace UI {

//
// Show - Display a message with automatic color based on type
//
// This is the main "entry point" for showing messages. You tell it what to say,
// how long to show it, how it should behave, and what kind of message it is.
// The class automatically picks the right color based on the type.
//
void StatusMessage::Show(const std::string& text, float duration, DisplayMode mode, Type type)
{
    text_ = text;
    color_ = GetColorForType(type);
    timer_ = duration;
    maxDuration_ = duration; // Store original duration for fade calculations
    mode_ = mode;
    visible_ = true;
}

//
// ShowCustom - Display a message with a custom color
//
// Same as Show(), but lets you pick your own color instead of using the
// standard Success/Error/Warning/Info colors.
//
void StatusMessage::ShowCustom(const std::string& text, const ImVec4& color, float duration, DisplayMode mode)
{
    text_ = text;
    color_ = color;
    timer_ = duration;
    maxDuration_ = duration;
    mode_ = mode;
    visible_ = true;
}

//
// Convenience methods - Quick shortcuts for common message types
//
// These save you from typing out all the parameters every time.
// Just call ShowSuccess("Done!") instead of Show("Done!", 3.0f, Timer, Success).
//

void StatusMessage::ShowSuccess(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Success);
}

void StatusMessage::ShowError(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Error);
}

void StatusMessage::ShowWarning(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Warning);
}

void StatusMessage::ShowInfo(const std::string& text, float duration, DisplayMode mode)
{
    Show(text, duration, mode, Type::Info);
}

void StatusMessage::RenderOverlay(float deltaTime)
{
    if (!visible_) {
        return;
    }

    timer_ -= deltaTime;

    float alpha = 1.0f;
    if (mode_ == DisplayMode::TimerWithFade && maxDuration_ > 0.0f) {
        alpha = timer_ / maxDuration_;
    }

    if (timer_ <= 0.0f && mode_ != DisplayMode::ManualDismiss) {
        visible_ = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    // Anchor to bottom-center, 15% up from bottom edge
    ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.85f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.72f * alpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

    if (ImGui::Begin("##ss_toast", nullptr, flags)) {
        ImVec4 fadedColor = color_;
        fadedColor.w *= alpha;
        ImGui::TextColored(fadedColor, "%s", text_.c_str());

        if (mode_ == DisplayMode::ManualDismiss) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Dismiss")) {
                visible_ = false;
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

//
// IMPORTANT: Call this EVERY FRAME in your UI render function!
// This is where all the magic happens - it draws the message and manages the timer.
//
// How it works:
// 1. If not visible, do nothing (message is hidden)
// 2. If visible, countdown the timer using deltaTime (time since last frame)
// 3. Display the message based on the mode:
//    - Timer: Show until timer expires, then hide instantly
//    - TimerWithFade: Show and gradually fade out as timer approaches 0
//    - ManualDismiss: Show with a "Dismiss" button, ignore timer
//
void StatusMessage::Render(float deltaTime)
{
    if (!visible_) {
        return; // Nothing to show
    }

    // Countdown the timer (subtract time since last frame)
    timer_ -= deltaTime;

    // Handle different display modes
    switch (mode_) {
        case DisplayMode::Timer:
            // Simple timer - show message until timer expires, then hide instantly
            if (timer_ > 0.0f) {
                ImGui::TextColored(color_, "%s", text_.c_str());
            } else {
                visible_ = false; // Timer expired, hide the message
            }
            break;

        case DisplayMode::TimerWithFade:
            // Timer with fade - gradually reduce opacity as timer approaches 0
            if (timer_ > 0.0f) {
                // Calculate fade: when timer = maxDuration, alpha = 1.0 (fully visible)
                //                 when timer = 0, alpha = 0.0 (fully transparent)
                float fadeAlpha = timer_ / maxDuration_;

                // Create faded color by multiplying the alpha channel
                ImVec4 fadedColor = color_;
                fadedColor.w *= fadeAlpha; // w is the alpha channel in ImVec4

                ImGui::TextColored(fadedColor, "%s", text_.c_str());
            } else {
                visible_ = false; // Fade complete, hide the message
            }
            break;

        case DisplayMode::ManualDismiss:
            // Manual dismiss - show message with a "Dismiss" button
            // Timer is ignored in this mode (message stays until user clicks button)
            ImGui::TextColored(color_, "%s", text_.c_str());
            ImGui::SameLine(); // Put the button on the same line as the text

            // Show a small dismiss button next to the message
            if (ImGui::SmallButton("Dismiss")) {
                visible_ = false; // User clicked dismiss, hide the message
            }
            break;
    }
}

//
// Clear - Immediately hide the message
//
// Use this to force the message to disappear right now, regardless of timer or mode.
// It's like hitting a "cancel" button - resets everything back to empty/hidden state.
//
void StatusMessage::Clear()
{
    text_.clear();
    color_ = ImVec4(0, 0, 0, 0); // Transparent black (invisible)
    timer_ = 0.0f;
    maxDuration_ = 0.0f;
    visible_ = false;
}

//
// IsVisible - Check if a message is currently showing
//
// Returns true if there's a message on screen, false if hidden.
//
bool StatusMessage::IsVisible() const
{
    return visible_;
}

//
// GetColorForType - Helper to map message type to color
//
// This is a private helper that returns the standard color for each message type.
// Think of it as a "color palette" for the different message categories.
//
// Colors chosen to match common UI conventions:
// - Success: Green (like a checkmark or "go" signal)
// - Error: Red (like a stop sign or warning light)
// - Warning: Yellow/Orange (like caution tape)
// - Info: Blue (like information signs)
//
ImVec4 StatusMessage::GetColorForType(Type type) const
{
    switch (type) {
        case Type::Success:
            return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Bright green (R=0, G=1, B=0, A=1)

        case Type::Error:
            return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Bright red (R=1, G=0, B=0, A=1)

        case Type::Warning:
            return ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Yellow/Orange (R=1, G=0.8, B=0, A=1)

        case Type::Info:
            return ImVec4(0.4f, 0.7f, 1.0f, 1.0f); // Light blue (R=0.4, G=0.7, B=1, A=1)

        default:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White (fallback, should never happen)
    }
}

} // namespace UI
```

## File: src/ui/StatusMessageUI.h
```c
#pragma once

// StatusMessage.h
//
// A reusable class for displaying temporary or dismissible status messages in the UI.
// This replaces 3 different timer/boolean patterns found in LoadoutUI, SettingsUI, and TrainingPackUI.
//
// Purpose: Provides a single, consistent way to show feedback messages to users
// (like "Loadout applied!", "Pack added!", "Error: invalid code") across all UI components.
//
// Think of it like a notification system - you can show a message, pick how long it shows,
// and choose whether it fades out automatically or waits for the user to dismiss it.
//
// Example Usage:
//   // In your UI class header file:
//   UI::StatusMessage statusMsg;
//
//   // When something happens (like saving successfully):
//   statusMsg.ShowSuccess("Loadout applied!", 3.0f);
//
//   // Every frame in your render function:
//   statusMsg.Render(ImGui::GetIO().DeltaTime);
//
// The class handles all the timer logic, color management, and display automatically.

#include "IMGUI/imgui.h"
#include <string>

namespace UI {

class StatusMessage
{
  public:
    //
    // DisplayMode - How the message should appear and disappear
    //
    // This controls the "lifecycle" of the message - how long it shows and how it hides.
    //
    enum class DisplayMode
    {
        // Timer - Shows for a set duration, then disappears instantly
        // Like a toast notification that just vanishes after 3 seconds
        // Best for: Quick confirmations like "Saved!" or "Refreshed"
        Timer,

        // TimerWithFade - Shows for a set duration, gradually fading to transparent
        // The message starts fully visible, then slowly becomes see-through before vanishing
        // Best for: Success messages that don't need immediate attention
        TimerWithFade,

        // ManualDismiss - Stays visible until user clicks a "Dismiss" button
        // Won't go away on its own - the user must actively close it
        // Best for: Important errors or warnings that need acknowledgment
        ManualDismiss
    };

    //
    // Type - The kind of message (determines the default color)
    //
    // This helps categorize messages and automatically picks an appropriate color.
    // Think of it like traffic lights: green = good, red = bad, yellow = warning.
    //
    enum class Type
    {
        Success, // Something worked! (Green color)
        Error,   // Something failed! (Red color)
        Warning, // Be careful! (Yellow color)
        Info     // Just letting you know... (Blue color)
    };

    // Constructor - creates an empty status message (nothing shows until you call Show...)
    StatusMessage() = default;

    //
    // Show - Display a message with automatic color based on type
    //
    // This is the main method for showing messages. It picks the color automatically
    // based on whether it's a success, error, warning, or info message.
    //
    // Parameters:
    //   text - The message to display (e.g., "Loadout applied successfully!")
    //   duration - How many seconds to show it (ignored if mode is ManualDismiss)
    //   mode - How it should appear/disappear (Timer, TimerWithFade, or ManualDismiss)
    //   type - What kind of message it is (Success, Error, Warning, or Info)
    //
    // Example:
    //   statusMsg.Show("Settings saved!", 3.0f, DisplayMode::Timer, Type::Success);
    //
    void Show(const std::string& text, float duration, DisplayMode mode, Type type);

    //
    // ShowCustom - Display a message with a custom color
    //
    // Use this when you want a specific color that doesn't match the standard types.
    // For example, if you want purple text or a specific shade of blue.
    //
    // Parameters:
    //   text - The message to display
    //   color - Custom ImVec4 color (RGBA values from 0.0 to 1.0)
    //   duration - How many seconds to show it (ignored if mode is ManualDismiss)
    //   mode - How it should appear/disappear
    //
    // Example:
    //   ImVec4 purple = ImVec4(0.8f, 0.4f, 1.0f, 1.0f);
    //   statusMsg.ShowCustom("Custom message!", purple, 5.0f, DisplayMode::TimerWithFade);
    //
    void ShowCustom(const std::string& text, const ImVec4& color, float duration, DisplayMode mode);

    //
    // Convenience methods - Shortcuts for common message types
    //
    // These are "helper methods" that make showing messages even easier.
    // Instead of typing out all the parameters, you can just call ShowSuccess(...)
    // and it automatically picks sensible defaults.
    //

    // ShowSuccess - Show a green success message
    // Defaults: 3 seconds, simple timer (instant hide)
    // Example: statusMsg.ShowSuccess("Pack added!");
    void ShowSuccess(const std::string& text, float duration = 3.0f, DisplayMode mode = DisplayMode::Timer);

    // ShowError - Show a red error message
    // Defaults: 3 seconds, manual dismiss (user must click "Dismiss")
    // Example: statusMsg.ShowError("Invalid pack code format");
    void ShowError(const std::string& text, float duration = 3.0f, DisplayMode mode = DisplayMode::ManualDismiss);

    // ShowWarning - Show a yellow warning message
    // Defaults: 5 seconds (longer than success), simple timer
    // Example: statusMsg.ShowWarning("Applying loadout...");
    void ShowWarning(const std::string& text, float duration = 5.0f, DisplayMode mode = DisplayMode::Timer);

    // ShowInfo - Show a blue informational message
    // Defaults: 2.5 seconds, fade effect (looks smooth)
    // Example: statusMsg.ShowInfo("Loadouts refreshed");
    void ShowInfo(const std::string& text, float duration = 2.5f, DisplayMode mode = DisplayMode::TimerWithFade);

    //
    // Render - Display the message on screen
    //
    // IMPORTANT: You must call this EVERY FRAME in your UI render function!
    // This is what actually draws the message and handles the timer countdown.
    //
    // If you don't call this, the message won't show up.
    // Think of it like watering a plant - you need to do it regularly.
    //
    // Parameters:
    //   deltaTime - Time since last frame in seconds (use ImGui::GetIO().DeltaTime)
    //
    // Example in your render function:
    //   void RenderMyUI() {
    //       // ... your other UI code ...
    //       statusMsg.Render(ImGui::GetIO().DeltaTime);
    //   }
    //
    void Render(float deltaTime);

    //
    // RenderOverlay - Display the message as a floating on-screen overlay
    //
    // Use this when calling from outside an ImGui window context (e.g., BakkesMod's
    // Render() callback). Creates its own semi-transparent window anchored to the
    // bottom-center of the screen. Safe to call every frame even when not visible.
    //
    // Parameters:
    //   deltaTime - Time since last frame in seconds (use ImGui::GetIO().DeltaTime)
    //
    void RenderOverlay(float deltaTime);

    //
    // Clear - Immediately hide the message
    //
    // Use this when you want to force the message to disappear right now,
    // regardless of its timer or mode. Like pressing a "cancel" button.
    //
    // Example:
    //   if (userClickedCancel) {
    //       statusMsg.Clear();  // Hide the message instantly
    //   }
    //
    void Clear();

    //
    // IsVisible - Check if a message is currently showing
    //
    // Returns true if there's a message on screen, false if it's hidden.
    // Useful for checking if you need to show a new message or if one is already active.
    //
    // Example:
    //   if (!statusMsg.IsVisible()) {
    //       statusMsg.ShowSuccess("New message!");
    //   }
    //
    bool IsVisible() const;

  private:
    // Internal state - these track what message is showing and how
    std::string text_;  // The current message text
    ImVec4 color_;      // The current message color (RGBA)
    float timer_;       // Countdown timer (seconds remaining)
    float maxDuration_; // Original duration (used for fade calculation)
    DisplayMode mode_;  // How the message should behave
    bool visible_;      // Is the message currently showing?

    // Helper method to get the standard color for a message type
    // (e.g., Type::Success returns green, Type::Error returns red)
    ImVec4 GetColorForType(Type type) const;
};

} // namespace UI
```

## File: src/ui/TrainingPackUI.cpp
```cpp
#include "pch.h"
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "TrainingPackUI.h"
#include "TrainingPackManager.h"
#include "SuiteSpot.h"
#include "SettingsSync.h"
#include "ConstantsUI.h"
#include "HelpersUI.h"
#include "bakkesmod/wrappers/http/HttpWrapper.h"
#include "IMGUI/SuiteSpotIcons.h"
#include "IMGUI/imgui_rangeslider.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <fstream>

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Sortable column header that shows asc/desc indicator and handles click
bool SortableColumnHeader(const char* label, int columnIndex, int& currentSortColumn, bool& sortAscending)
{
    char buffer[256];
    if (currentSortColumn == columnIndex)
        snprintf(buffer, sizeof(buffer), "%s %s", label, sortAscending ? "(asc)" : "(desc)");
    else
        snprintf(buffer, sizeof(buffer), "%s", label);

    bool clicked = ImGui::Selectable(buffer, currentSortColumn == columnIndex, ImGuiSelectableFlags_DontClosePopups);
    if (clicked) {
        if (currentSortColumn == columnIndex)
            sortAscending = !sortAscending;
        else {
            currentSortColumn = columnIndex;
            sortAscending = true;
        }
    }
    return clicked;
}

// Draw a small colored text badge (no button, just colored text in brackets)
void DrawTagBadge(const char* tag)
{
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::TAG_TEXT_COLOR);
    ImGui::Text("[%s]", tag);
    ImGui::PopStyleColor();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// DifficultyColor (static helper used in both panels)
// ─────────────────────────────────────────────────────────────────────────────
/*static*/ ImVec4 TrainingPackUI::DifficultyColor(const std::string& difficulty)
{
    if (difficulty == "Bronze") return UI::TrainingPackUI::DIFFICULTY_BADGE_BRONZE_COLOR;
    if (difficulty == "Silver") return UI::TrainingPackUI::DIFFICULTY_BADGE_SILVER_COLOR;
    if (difficulty == "Gold") return UI::TrainingPackUI::DIFFICULTY_BADGE_GOLD_COLOR;
    if (difficulty == "Platinum") return UI::TrainingPackUI::DIFFICULTY_BADGE_PLATINUM_COLOR;
    if (difficulty == "Diamond") return UI::TrainingPackUI::DIFFICULTY_BADGE_DIAMOND_COLOR;
    if (difficulty == "Champion") return UI::TrainingPackUI::DIFFICULTY_BADGE_CHAMPION_COLOR;
    if (difficulty == "Grand Champion") return UI::TrainingPackUI::DIFFICULTY_BADGE_GRAND_CHAMPION_COLOR;
    if (difficulty == "Supersonic Legend") return UI::TrainingPackUI::DIFFICULTY_BADGE_SUPERSONIC_LEGEND_COLOR;
    return UI::TrainingPackUI::DIFFICULTY_BADGE_UNRANKED_COLOR;
}

// ─────────────────────────────────────────────────────────────────────────────
// ExtractYouTubeId
// ─────────────────────────────────────────────────────────────────────────────
/*static*/ std::string TrainingPackUI::ExtractYouTubeId(const std::string& url)
{
    if (url.empty()) return {};

    // youtu.be/{ID}
    auto pos = url.find("youtu.be/");
    if (pos != std::string::npos) {
        pos += 9;
        auto end = url.find_first_of("?&", pos);
        return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    // youtube.com/shorts/{ID}
    pos = url.find("/shorts/");
    if (pos != std::string::npos) {
        pos += 8;
        auto end = url.find_first_of("?&", pos);
        return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    // youtube.com/watch?v={ID}
    pos = url.find("v=");
    if (pos != std::string::npos) {
        pos += 2;
        auto end = url.find_first_of("&", pos);
        return url.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// ExtractImgurId — returns imgur image ID from https://i.imgur.com/{id}.mp4
// ─────────────────────────────────────────────────────────────────────────────
/*static*/ std::string TrainingPackUI::ExtractImgurId(const std::string& url)
{
    if (url.empty()) return {};
    auto pos = url.rfind('/');
    if (pos == std::string::npos) return {};
    std::string filename = url.substr(pos + 1);
    // Strip extension
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) filename = filename.substr(0, dot);
    return filename;
}

// ─────────────────────────────────────────────────────────────────────────────
// FetchThumbnailForSelected
// Prefers gifUrl (imgur preview) over videoUrl (YouTube thumbnail)
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::FetchThumbnailForSelected()
{
    if (selectedPackCode.empty() || selectedPackCode == lastFetchedThumbnailCode_) return;
    lastFetchedThumbnailCode_ = selectedPackCode;

    TrainingEntry* entry = nullptr;
    for (auto& p : filteredPacks) {
        if (p.code == selectedPackCode) {
            entry = &p;
            break;
        }
    }
    if (!entry) return;
    if (entry->gifUrl.empty() && entry->videoUrl.empty()) return;
    if (entry->isThumbnailRequested) return;

    entry->isThumbnailRequested = true;

    // Prefer imgur gif preview; fall back to YouTube thumbnail
    std::string thumbUrl;
    std::string cacheKey;
    std::string clickUrl;

    if (!entry->gifUrl.empty()) {
        std::string imgurId = ExtractImgurId(entry->gifUrl);
        if (!imgurId.empty()) {
            // imgur large thumbnail: {id}l.jpg
            thumbUrl = "https://i.imgur.com/" + imgurId + "l.jpg";
            cacheKey = "imgur_" + imgurId;
            clickUrl = entry->gifUrl; // open the mp4 directly
        }
    }

    if (thumbUrl.empty() && !entry->videoUrl.empty()) {
        std::string videoId = ExtractYouTubeId(entry->videoUrl);
        if (!videoId.empty()) {
            thumbUrl = "https://img.youtube.com/vi/" + videoId + "/mqdefault.jpg";
            cacheKey = "yt_" + videoId;
            clickUrl = entry->videoUrl;
        }
    }

    if (thumbUrl.empty()) return;

    std::filesystem::path cachedPath = thumbnailCacheDir_ / (cacheKey + ".jpg");
    if (std::filesystem::exists(cachedPath)) {
        entry->thumbnailImage = std::make_shared<ImageWrapper>(cachedPath.string(), false, true);
        return;
    }

    std::filesystem::create_directories(thumbnailCacheDir_);
    std::string cachePath = cachedPath.string();
    std::string code = selectedPackCode;
    TrainingPackUI* self = this;

    CurlRequest req;
    req.url = thumbUrl;
    HttpWrapper::SendCurlRequest(req, [self, cachePath, code](int httpCode, char* data, size_t size) {
        if (httpCode != 200 || size == 0) return;
        std::ofstream f(cachePath, std::ios::binary);
        if (!f) return;
        f.write(data, static_cast<std::streamsize>(size));
        f.close();
        for (auto& p : self->filteredPacks) {
            if (p.code == code) {
                p.thumbnailImage = std::make_shared<ImageWrapper>(cachePath, false, true);
                break;
            }
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / PluginWindow boilerplate
// ─────────────────────────────────────────────────────────────────────────────
TrainingPackUI::TrainingPackUI(SuiteSpot* plugin) : plugin_(plugin)
{
    thumbnailCacheDir_ = plugin_->GetDataRoot() / "SuiteSpot" / "ThumbnailCache";
    auto iconPath = plugin_->GetDataRoot() / "SuiteSpot" / "Resources" / "Icons" / "icon_youtube.png";
    youtubeIcon_ = std::make_shared<ImageWrapper>(iconPath.string(), true);
    youtubeIcon_->LoadForImGui([](bool) {});
}

std::string TrainingPackUI::GetMenuName()
{
    return "suitespot_browser";
}
std::string TrainingPackUI::GetMenuTitle()
{
    return "SuiteSpot Training Browser";
}
void TrainingPackUI::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool TrainingPackUI::ShouldBlockInput()
{
    if (!isWindowOpen_) return false;
    ImGuiIO& io = ImGui::GetIO();
    return io.WantTextInput && ImGui::IsAnyItemActive();
}

bool TrainingPackUI::IsActiveOverlay()
{
    return isWindowOpen_;
}

void TrainingPackUI::OnOpen()
{
    isWindowOpen_ = true;
    needsFocusOnNextRender_ = true;
}

void TrainingPackUI::OnClose()
{
    isWindowOpen_ = false;
}
bool TrainingPackUI::IsOpen()
{
    return isWindowOpen_;
}
void TrainingPackUI::SetOpen(bool open)
{
    isWindowOpen_ = open;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render — main entry point
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::Render()
{
    if (!isWindowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);

    if (needsFocusOnNextRender_) {
        ImGui::SetNextWindowFocus();
        needsFocusOnNextRender_ = false;
    }

    // ── Style push (11 vars, 17 colors) ──────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::INTERACTIVE_FRAME_BORDER_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, UI::INTERACTIVE_FRAME_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::FRAME_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, UI::ITEM_SPACING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, UI::WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, UI::CHILD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, UI::CHILD_BORDER_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, UI::TAB_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, UI::GRAB_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, UI::GRAB_MIN_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, UI::SCROLLBAR_ROUNDING);

    ImGui::PushStyleColor(ImGuiCol_Border, UI::INTERACTIVE_BORDER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button, UI::BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::BUTTON_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::BUTTON_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, UI::FRAME_BG_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, UI::FRAME_BG_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UI::FRAME_BG_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, UI::CHECKMARK_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Header, UI::HEADER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::HEADER_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::HEADER_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Tab, UI::TAB_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, UI::TAB_HOVER_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabActive, UI::TAB_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused, UI::TAB_UNFOCUSED_COLOR);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, UI::TAB_UNFOCUSED_ACTIVE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::CHILD_BG_COLOR);

    if (!ImGui::Begin(GetMenuTitle().c_str(), &isWindowOpen_)) {
        ImGui::PopStyleColor(17);
        ImGui::PopStyleVar(11);
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(UI::FONT_SCALE);

    const auto* manager = plugin_->trainingPackMgr.get();
    static const std::vector<TrainingEntry> emptyPacks;
    const auto& packs = manager ? manager->GetPacks() : emptyPacks;
    const int packCount = manager ? manager->GetPackCount() : 0;
    const auto& lastUpdated = manager ? manager->GetLastUpdated() : std::string{};
    const bool scraping = manager && manager->IsScrapingInProgress();

    // ── Sync selection from Quick Picks ──────────────────────────────────────
    if (plugin_->settingsSync) {
        std::string qp = plugin_->settingsSync->GetQuickPicksSelectedCode();
        if (qp != lastQuickPicksSelected) {
            if (!qp.empty()) selectedPackCode = qp;
            lastQuickPicksSelected = qp;
        }
    }

    // ── Window header row ─────────────────────────────────────────────────────
    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Training Pack Browser");
    ImGui::SameLine();
    if (packCount > 0) {
        ImGui::TextDisabled("  %d packs", packCount);
        if (!lastUpdated.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("| Updated: %s", lastUpdated.c_str());
        }
    }

    // Right-aligned control buttons
    float btnW = ImGui::CalcTextSize("Reload Cache").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
    float updateW = ImGui::CalcTextSize("Update Pack List").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - btnW - updateW - ImGui::GetStyle().ItemSpacing.x);

    if (scraping) {
        ImGui::TextColored(UI::TrainingPackUI::SCRAPING_STATUS_TEXT_COLOR, "Updating...");
        ImGui::SameLine();
    } else {
        if (ImGui::Button("Update Pack List")) plugin_->UpdateTrainingPackList();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Download latest training packs (~2-3 minutes)");
        ImGui::SameLine();
    }
    if (ImGui::Button("Reload Cache")) {
        plugin_->LoadTrainingPacksFromFile(plugin_->GetTrainingPacksPath());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reload packs from cached JSON");

    ImGui::Separator();
    ImGui::Spacing();

    if (packs.empty()) {
        ImGui::TextWrapped("No packs available. Click 'Update Pack List' to download the training pack database.");
        ImGui::PopStyleColor(17);
        ImGui::PopStyleVar(11);
        ImGui::End();
        return;
    }

    // ── Filter bar (full width above both panels) ─────────────────────────────
    bool filtersChanged = (strcmp(packSearchText, lastSearchText) != 0) ||
                          (packDifficultyFilter != lastDifficultyFilter) || (packTagFilter != lastTagFilter) ||
                          (packMinShots != lastMinShots) || (packMaxShots != lastMaxShots) ||
                          (packVideoFilter != lastVideoFilter) || (packSortColumn != lastSortColumn) ||
                          (packSortAscending != lastSortAscending);

    // Row 1: search + difficulty + tags + has-video + clear
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputTextWithHint("##search", "Search name, creator, tag...", packSearchText, IM_ARRAYSIZE(packSearchText)))
        filtersChanged = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Search by pack name, creator, or tag");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    static const char* difficulties[] = {"All",      "Unranked", "Bronze",   "Silver",         "Gold",
                                         "Platinum", "Diamond",  "Champion", "Grand Champion", "Supersonic Legend"};
    if (ImGui::BeginCombo("##diff", packDifficultyFilter.c_str())) {
        for (auto& d : difficulties) {
            bool sel = (packDifficultyFilter == d);
            if (ImGui::Selectable(d, sel)) {
                packDifficultyFilter = d;
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter by difficulty");

    ImGui::SameLine();
    bool packsSourceChanged = (lastPackCount != packCount);
    if (!tagsInitialized || packsSourceChanged) {
        if (manager)
            manager->BuildAvailableTags(availableTags);
        else {
            availableTags.clear();
            availableTags.push_back("All Tags");
        }
        tagsInitialized = true;
        lastPackCount = packCount;
    }
    std::string displayTag = packTagFilter.empty() ? "All Tags" : packTagFilter;
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##tags", displayTag.c_str())) {
        for (const auto& tag : availableTags) {
            bool sel = (tag == displayTag);
            if (ImGui::Selectable(tag.c_str(), sel)) {
                packTagFilter = (tag == "All Tags") ? "" : tag;
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter by tag");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::RangeSliderInt("Shots##filter", &packMinShots, &packMaxShots, 0, 50, "(%d-%d shots)")) {
        if (packMinShots > packMaxShots) packMaxShots = packMinShots;
        filtersChanged = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Filter by shot count range (packs with unknown shot count always shown)");

    ImGui::SameLine();
    if (ImGui::Checkbox("Has Video", &packVideoFilter)) filtersChanged = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show only packs with video tutorial links");

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        packSearchText[0] = '\0';
        packDifficultyFilter = "All";
        packTagFilter = "";
        packMinShots = 0;
        packMaxShots = 50;
        packVideoFilter = false;
        filtersChanged = true;
    }

    // Showing count
    ImGui::SameLine();
    ImGui::TextDisabled("(%d shown)", (int)filteredPacks.size());

    ImGui::Spacing();

    // ── Rebuild filtered list when needed ────────────────────────────────────
    if (filtersChanged || packsSourceChanged || !packListInitialized) {
        if (manager) {
            manager->FilterAndSortPacks(packSearchText, packDifficultyFilter, packTagFilter, packMinShots, packMaxShots,
                                        packVideoFilter, packSortColumn, packSortAscending, filteredPacks);
        } else {
            filteredPacks.clear();
        }
        strncpy_s(lastSearchText, packSearchText, sizeof(lastSearchText) - 1);
        lastDifficultyFilter = packDifficultyFilter;
        lastTagFilter = packTagFilter;
        lastMinShots = packMinShots;
        lastMaxShots = packMaxShots;
        lastVideoFilter = packVideoFilter;
        lastSortColumn = packSortColumn;
        lastSortAscending = packSortAscending;
        packListInitialized = true;
    }

    // ── Status bar ───────────────────────────────────────────────────────────
    browserStatus.Render(ImGui::GetIO().DeltaTime);
    if (browserStatus.IsVisible()) ImGui::Spacing();

    // ── Two-panel layout ─────────────────────────────────────────────────────
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(UI::PackBrowserUI::LEFT_PANEL_MIN_WIDTH,
                               availWidth * UI::PackBrowserUI::LEFT_PANEL_WIDTH_PERCENT);
    float rightWidth = availWidth - leftWidth - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup();

    // ─────────────────────────────────────────────────────────────────────────
    // LEFT PANEL: list
    // ─────────────────────────────────────────────────────────────────────────
    if (ImGui::BeginChild("PackList", ImVec2(leftWidth, UI::PackBrowserUI::BROWSER_HEIGHT), true, ImGuiWindowFlags_None)) {
        // Frozen column header row
        ImGui::Columns(3, "PackHdr", true);
        ImGui::SetColumnWidth(0, leftWidth * 0.55f);
        ImGui::SetColumnWidth(1, leftWidth * 0.25f);
        // col 2 = remaining

        if (SortableColumnHeader("Name", 0, packSortColumn, packSortAscending)) filtersChanged = true;
        ImGui::NextColumn();
        if (SortableColumnHeader("Difficulty", 2, packSortColumn, packSortAscending)) filtersChanged = true;
        ImGui::NextColumn();
        if (SortableColumnHeader("Shots", 3, packSortColumn, packSortAscending)) filtersChanged = true;
        ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::Separator();

        // Scrollable rows
        if (ImGui::BeginChild("PackRows", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4.0f), false)) {
            ImGui::Columns(3, "PackBody", true);
            ImGui::SetColumnWidth(0, leftWidth * 0.55f);
            ImGui::SetColumnWidth(1, leftWidth * 0.25f);

            ImGuiListClipper clipper;
            clipper.Begin((int)filteredPacks.size());
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    auto& pack = filteredPacks[row];
                    bool isSelected = (selectedPackCode == pack.code);

                    ImGui::PushID(pack.code.c_str());

                    // Video indicator: YouTube icon for video, small orange dot for gif-only
                    if (!pack.videoUrl.empty()) {
                        float iconSize = ImGui::GetTextLineHeight();
                        if (youtubeIcon_ && youtubeIcon_->IsLoadedForImGui()) {
                            ImGui::Image(youtubeIcon_->GetImGuiTex(), ImVec2(iconSize, iconSize));
                        } else {
                            ImVec2 dotPos = ImGui::GetCursorScreenPos();
                            float r = iconSize * 0.22f;
                            dotPos.x += r + 1.0f;
                            dotPos.y += iconSize * 0.5f;
                            ImGui::GetWindowDrawList()->AddCircleFilled(dotPos, r, IM_COL32(220, 50, 50, 220));
                            ImGui::Dummy(ImVec2(iconSize, iconSize));
                        }
                        ImGui::SameLine(0, 3.0f);
                    } else if (!pack.gifUrl.empty()) {
                        // gif-only pack: small orange dot
                        float iconSize = ImGui::GetTextLineHeight();
                        ImVec2 dotPos = ImGui::GetCursorScreenPos();
                        float r = iconSize * 0.22f;
                        dotPos.x += r + 1.0f;
                        dotPos.y += iconSize * 0.5f;
                        ImGui::GetWindowDrawList()->AddCircleFilled(dotPos, r, IM_COL32(255, 160, 30, 220));
                        ImGui::Dummy(ImVec2(iconSize, iconSize));
                        ImGui::SameLine(0, 3.0f);
                    }

                    if (ImGui::Selectable(pack.name.c_str(), isSelected,
                                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                        selectedPackCode = pack.code;
                        FetchThumbnailForSelected();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                    // Scroll to item when it becomes selected
                    if (isSelected && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);

                    // Context menu
                    if (ImGui::BeginPopupContextItem("##ctx")) {
                        ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "%s", pack.name.c_str());
                        ImGui::Separator();
                        if (ImGui::Selectable("Set Post-Match")) {
                            plugin_->settingsSync->SetQuickPicksSelected(pack.code);
                            plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(pack.code);
                            plugin_->settingsSync->SetCurrentTrainingCode(pack.code);
                            plugin_->cvarManager->getCvar("suitespot_current_training_code").setValue(pack.code);
                            browserStatus.ShowSuccess("Post-Match set: " + pack.name, 2.0f,
                                                      UI::StatusMessage::DisplayMode::TimerWithFade);
                        }
                        if (ImGui::Selectable("Load Now")) {
                            LoadPackImmediately(pack.code);
                        }
                        if (!pack.videoUrl.empty() && ImGui::Selectable("Watch Video")) {
                            ShellExecuteA(NULL, "open", pack.videoUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        }
                        ImGui::EndPopup();
                    }

                    // Drag source for bag manager
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("TRAINING_PACK_CODE", pack.code.c_str(), pack.code.length() + 1);
                        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Dragging: %s", pack.name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    ImGui::PopID();
                    ImGui::NextColumn();

                    // Difficulty column
                    std::string diff = pack.difficulty.empty() ? "Unranked" : pack.difficulty;
                    ImGui::TextColored(DifficultyColor(diff), "%s", diff.c_str());
                    ImGui::NextColumn();

                    // Shots column
                    if (pack.shotCount > 0)
                        ImGui::Text("%d", pack.shotCount);
                    else
                        ImGui::TextDisabled("-");
                    ImGui::NextColumn();
                }
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();

        // Left panel footer: "+ Add Custom Pack" button
        ImGui::Separator();
        if (ImGui::Button("+ Add Custom Pack", ImVec2(-1.0f, 0.0f))) {
            showAddPackModal_ = true;
            ImGui::OpenPopup("Add Custom Pack");
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ─────────────────────────────────────────────────────────────────────────
    // RIGHT PANEL: detail
    // ─────────────────────────────────────────────────────────────────────────
    if (ImGui::BeginChild("PackDetail", ImVec2(rightWidth, UI::PackBrowserUI::BROWSER_HEIGHT), true,
                          ImGuiWindowFlags_None)) {
        // Find selected pack
        const TrainingEntry* selectedPack = nullptr;
        for (auto& p : filteredPacks) {
            if (p.code == selectedPackCode) {
                selectedPack = &p;
                break;
            }
        }
        RenderDetailPanel(selectedPack);
    }
    ImGui::EndChild();

    ImGui::EndGroup();

    // ── Modal: Add Custom Pack ────────────────────────────────────────────────
    RenderCustomPackModal();

    ImGui::PopStyleColor(17);
    ImGui::PopStyleVar(11);
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// RenderDetailPanel
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::RenderDetailPanel(const TrainingEntry* pack)
{
    if (!pack) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avail.y * 0.40f);
        ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::NO_SELECTION_COLOR);
        float textW = ImGui::CalcTextSize("Select a pack from the list").x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - textW) * 0.5f);
        ImGui::TextUnformatted("Select a pack from the list");
        ImGui::PopStyleColor();
        return;
    }

    float panelW = ImGui::GetContentRegionAvail().x;

    // ── Thumbnail ────────────────────────────────────────────────────────────
    float thumbW = std::min(UI::PackBrowserUI::THUMBNAIL_WIDTH, panelW);
    float thumbH = thumbW * (UI::PackBrowserUI::THUMBNAIL_HEIGHT / UI::PackBrowserUI::THUMBNAIL_WIDTH);

    // Determine what to open when the thumbnail is clicked
    // Priority: gifUrl (imgur preview clip) → videoUrl (YouTube tutorial)
    const bool hasGif = !pack->gifUrl.empty();
    const bool hasVideo = !pack->videoUrl.empty();
    const bool hasPreview = hasGif || hasVideo;
    const std::string& previewClickUrl = hasGif ? pack->gifUrl : pack->videoUrl;
    const char* previewTooltip = hasGif ? "Open preview clip in browser" : "Open video in browser";

    if (pack->thumbnailImage && pack->thumbnailImage->GetImGuiTex()) {
        float offsetX = (panelW - thumbW) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::Image(pack->thumbnailImage->GetImGuiTex(), ImVec2(thumbW, thumbH));
        if (hasPreview) {
            ImGui::SetItemAllowOverlap();
            ImVec2 btnMin = ImGui::GetItemRectMin();
            ImGui::SetCursorScreenPos(btnMin);
            if (ImGui::InvisibleButton("##thumb_click", ImVec2(thumbW, thumbH))) {
                ShellExecuteA(NULL, "open", previewClickUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("%s", previewTooltip);
            }
        }
    } else {
        // Placeholder rect with centered text
        ImVec2 p = ImGui::GetCursorScreenPos();
        float offsetX = (panelW - thumbW) * 0.5f;
        p.x += offsetX;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + thumbW, p.y + thumbH), ImColor(35, 38, 48, 255), 4.0f);
        dl->AddRect(p, ImVec2(p.x + thumbW, p.y + thumbH), ImColor(60, 70, 90, 255), 4.0f);
        if (hasPreview) {
            const char* msg = pack->isThumbnailRequested ? "Loading..." : "No Preview";
            float tw = ImGui::CalcTextSize(msg).x;
            dl->AddText(ImVec2(p.x + (thumbW - tw) * 0.5f, p.y + thumbH * 0.5f - 7.0f), ImColor(100, 110, 130, 255), msg);
        } else {
            const char* msg = "No Video";
            float tw = ImGui::CalcTextSize(msg).x;
            dl->AddText(ImVec2(p.x + (thumbW - tw) * 0.5f, p.y + thumbH * 0.5f - 7.0f), ImColor(80, 85, 100, 255), msg);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        if (hasPreview) {
            if (ImGui::InvisibleButton("##thumb_click_ph", ImVec2(thumbW, thumbH))) {
                ShellExecuteA(NULL, "open", previewClickUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("%s", previewTooltip);
            }
        } else {
            ImGui::Dummy(ImVec2(thumbW, thumbH));
        }
    }

    ImGui::Spacing();

    // ── Pack name ─────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::PACK_NAME_COLOR);
    ImGui::PushTextWrapPos(panelW);
    ImGui::TextWrapped("%s", pack->name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // ── Creator / difficulty / shots row ─────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::CREATOR_COLOR);
    if (!pack->creator.empty()) ImGui::Text("By: %s", pack->creator.c_str());
    ImGui::PopStyleColor();

    std::string diff = pack->difficulty.empty() ? "Unranked" : pack->difficulty;
    ImGui::TextColored(DifficultyColor(diff), "%s", diff.c_str());
    ImGui::SameLine();
    if (pack->shotCount > 0)
        ImGui::TextDisabled(" · %d shots", pack->shotCount);
    else
        ImGui::TextDisabled(" · shots unknown");

    // ── Stats row ─────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::STATS_COLOR);
    ImGui::Text("Likes: %d  Plays: %d", pack->likes, pack->plays);
    ImGui::PopStyleColor();

    // ── Personal play count ───────────────────────────────────────────────────
    if (plugin_->usageTracker) {
        int myPlays = plugin_->usageTracker->GetLoadCount(pack->code);
        if (myPlays > 0) {
            int64_t ts = plugin_->usageTracker->GetLastPlayedTimestamp(pack->code);
            char dateBuf[32] = {};
            if (ts > 0) {
                std::time_t t = static_cast<std::time_t>(ts);
                std::tm tm_s{};
                localtime_s(&tm_s, &t);
                std::strftime(dateBuf, sizeof(dateBuf), "%b %d, %Y", &tm_s);
            }
            if (dateBuf[0])
                ImGui::TextDisabled("You've played this %d time%s (last: %s)", myPlays, myPlays == 1 ? "" : "s", dateBuf);
            else
                ImGui::TextDisabled("You've played this %d time%s", myPlays, myPlays == 1 ? "" : "s");
        } else {
            ImGui::TextDisabled("You haven't played this yet");
        }
    }

    // ── Tags ──────────────────────────────────────────────────────────────────
    if (!pack->tags.empty()) {
        ImGui::Spacing();
        for (size_t i = 0; i < pack->tags.size(); i++) {
            if (i > 0) ImGui::SameLine(0, 4.0f);
            DrawTagBadge(pack->tags[i].c_str());
        }
    }

    // ── Pack code (small, copyable) ───────────────────────────────────────────
    ImGui::Spacing();
    ImGui::TextDisabled("%s", pack->code.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to copy code");
        if (ImGui::IsItemClicked()) {
            ImGui::SetClipboardText(pack->code.c_str());
            browserStatus.ShowSuccess("Code copied!", 1.5f, UI::StatusMessage::DisplayMode::TimerWithFade);
        }
    }

    // ── Staff comments / notes ────────────────────────────────────────────────
    if (!pack->staffComments.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, UI::PackBrowserUI::COMMENTS_COLOR);
        ImGui::PushTextWrapPos(panelW);
        ImGui::TextWrapped("%s", pack->staffComments.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    // ── Action buttons ────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool isCurrentPostMatch = plugin_->settingsSync && plugin_->settingsSync->GetQuickPicksSelectedCode() == pack->code;

    if (isCurrentPostMatch) {
        ImGui::TextColored(UI::PackBrowserUI::SELECTED_BADGE_COLOR, "Selected for Post-Match");
        ImGui::Spacing();
    }

    float btnH = UI::PackBrowserUI::ACTION_BUTTON_HEIGHT;
    float halfW = (panelW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (!isCurrentPostMatch) {
        if (ImGui::Button("Set Post-Match", ImVec2(halfW, btnH))) {
            plugin_->settingsSync->SetQuickPicksSelected(pack->code);
            plugin_->cvarManager->getCvar("suitespot_quickpicks_selected").setValue(pack->code);
            plugin_->settingsSync->SetCurrentTrainingCode(pack->code);
            plugin_->cvarManager->getCvar("suitespot_current_training_code").setValue(pack->code);
            browserStatus.ShowSuccess("Post-Match set: " + pack->name, 2.0f,
                                      UI::StatusMessage::DisplayMode::TimerWithFade);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set this pack to load after matches");
        ImGui::SameLine();
    }

    if (ImGui::Button("Load Now", ImVec2(-1.0f, btnH))) {
        LoadPackImmediately(pack->code);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load this pack immediately");

    // Delete button (custom packs only)
    if (pack->source == "custom") {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, UI::TrainingPackUI::DELETE_BUTTON_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::TrainingPackUI::DELETE_BUTTON_HOVER_COLOR);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::TrainingPackUI::DELETE_BUTTON_BG_COLOR);
        if (ImGui::Button("Delete Custom Pack", ImVec2(-1.0f, btnH))) {
            if (plugin_->trainingPackMgr) {
                plugin_->trainingPackMgr->DeletePack(pack->code);
                browserStatus.ShowSuccess("Deleted custom pack", 3.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                selectedPackCode.clear();
            }
        }
        ImGui::PopStyleColor(3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RenderCustomPackModal
// ─────────────────────────────────────────────────────────────────────────────
void TrainingPackUI::RenderCustomPackModal()
{
    // Center the modal
    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Add Custom Pack", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextColored(UI::TrainingPackUI::SECTION_HEADER_TEXT_COLOR, "Add Custom Training Pack");
    ImGui::Separator();
    ImGui::Spacing();

    customPackStatus.Render(ImGui::GetIO().DeltaTime);
    if (customPackStatus.IsVisible()) ImGui::Spacing();

    // Code
    ImGui::TextUnformatted("Code *");
    ImGui::SameLine();
    ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "(XXXX-XXXX-XXXX-XXXX)");
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputText("##mcode", customPackCode, IM_ARRAYSIZE(customPackCode))) {
        // Auto-format: keep only alphanumeric, insert dashes at positions 4,9,14
        std::string raw;
        for (int i = 0; customPackCode[i]; i++) {
            char c = customPackCode[i];
            if (isalnum(static_cast<unsigned char>(c)))
                raw += static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        if (raw.length() > 16) raw = raw.substr(0, 16);
        std::string fmt;
        for (size_t i = 0; i < raw.length(); i++) {
            if (i > 0 && i % 4 == 0) fmt += '-';
            fmt += raw[i];
        }
        strncpy_s(customPackCode, fmt.c_str(), sizeof(customPackCode) - 1);
    }

    // Name
    ImGui::TextUnformatted("Name *");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##mname", customPackName, IM_ARRAYSIZE(customPackName));

    // Creator
    ImGui::TextUnformatted("Creator");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##mcreator", customPackCreator, IM_ARRAYSIZE(customPackCreator));

    // Difficulty + Shot Count on same row
    ImGui::TextUnformatted("Difficulty");
    ImGui::SameLine(120.0f);
    ImGui::TextUnformatted("Shot Count");

    static const char* diffs[] = {"Unranked", "Bronze",         "Silver",           "Gold", "Platinum", "Diamond",
                                  "Champion", "Grand Champion", "Supersonic Legend"};
    ImGui::SetNextItemWidth(150.0f);
    ImGui::Combo("##mdiff", &customPackDifficulty, diffs, IM_ARRAYSIZE(diffs));
    ImGui::SameLine(120.0f);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderInt("##mshots", &customPackShotCount, 1, 50);

    // Tags
    ImGui::TextUnformatted("Tags");
    ImGui::SameLine();
    ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "(comma-separated)");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##mtags", customPackTags, IM_ARRAYSIZE(customPackTags));

    // Notes
    ImGui::TextUnformatted("Notes");
    ImGui::InputTextMultiline("##mnotes", customPackNotes, IM_ARRAYSIZE(customPackNotes), ImVec2(440.0f, 60.0f));

    // Video URL
    ImGui::TextUnformatted("Video URL");
    ImGui::SetNextItemWidth(350.0f);
    ImGui::InputText("##mvideo", customPackVideoUrl, IM_ARRAYSIZE(customPackVideoUrl));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(UI::TrainingPackUI::DISABLED_INFO_TEXT_COLOR, "* Required");
    ImGui::SameLine();

    // Buttons right-aligned
    float addW = ImGui::CalcTextSize("Add Pack").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float cancelW = ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - addW - cancelW - ImGui::GetStyle().ItemSpacing.x);

    if (ImGui::Button("Add Pack", ImVec2(addW, 0))) {
        customPackStatus.Clear();
        if (strlen(customPackCode) == 0)
            customPackStatus.ShowError("Pack code is required");
        else if (!ValidatePackCode(customPackCode))
            customPackStatus.ShowError("Invalid format. Expected: XXXX-XXXX-XXXX-XXXX");
        else if (strlen(customPackName) == 0)
            customPackStatus.ShowError("Pack name is required");
        else {
            TrainingEntry p;
            p.code = customPackCode;
            p.name = customPackName;
            p.creator = strlen(customPackCreator) > 0 ? customPackCreator : "Unknown";
            p.difficulty = diffs[customPackDifficulty];
            p.shotCount = customPackShotCount;
            if (strlen(customPackTags) > 0) {
                std::string tagsStr = customPackTags;
                size_t start = 0, end = tagsStr.find(',');
                while (end != std::string::npos) {
                    auto tag = tagsStr.substr(start, end - start);
                    auto f = tag.find_first_not_of(" \t");
                    auto l = tag.find_last_not_of(" \t");
                    if (f != std::string::npos) p.tags.push_back(tag.substr(f, l - f + 1));
                    start = end + 1;
                    end = tagsStr.find(',', start);
                }
                auto tag = tagsStr.substr(start);
                auto f = tag.find_first_not_of(" \t"), l = tag.find_last_not_of(" \t");
                if (f != std::string::npos) p.tags.push_back(tag.substr(f, l - f + 1));
            }
            p.staffComments = customPackNotes;
            p.videoUrl = customPackVideoUrl;
            p.source = "custom";
            if (plugin_->trainingPackMgr && plugin_->trainingPackMgr->AddCustomPack(p)) {
                browserStatus.ShowSuccess("Pack added: " + p.name, 3.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
                ClearCustomPackForm();
                ImGui::CloseCurrentPopup();
            } else {
                customPackStatus.ShowError("Pack with this code already exists");
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(cancelW, 0))) {
        ClearCustomPackForm();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────────────────────
bool TrainingPackUI::ValidatePackCode(const char* code) const
{
    if (strlen(code) != 19) return false;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 9 || i == 14) {
            if (code[i] != '-') return false;
        } else {
            if (!isalnum(static_cast<unsigned char>(code[i]))) return false;
        }
    }
    return true;
}

void TrainingPackUI::ClearCustomPackForm()
{
    customPackCode[0] = '\0';
    customPackName[0] = '\0';
    customPackCreator[0] = '\0';
    customPackDifficulty = 0;
    customPackShotCount = 10;
    customPackTags[0] = '\0';
    customPackNotes[0] = '\0';
    customPackVideoUrl[0] = '\0';
    customPackStatus.Clear();
}

void TrainingPackUI::LoadPackImmediately(const std::string& packCode)
{
    if (packCode.empty() || !plugin_) return;
    if (plugin_->usageTracker) plugin_->usageTracker->IncrementLoadCount(packCode);

    std::string packName = packCode;
    if (const auto* mgr = plugin_->trainingPackMgr.get()) {
        for (const auto& p : mgr->GetPacks()) {
            if (p.code == packCode) {
                packName = p.name;
                break;
            }
        }
    }

    SuiteSpot* plug = plugin_;
    std::string code = packCode;
    plug->gameWrapper->SetTimeout(
        [plug, code, packName](GameWrapper*) {
            plug->cvarManager->executeCommand("load_training " + code);
            LOG("SuiteSpot: Loading training pack: {}", packName);
        },
        0.0f);

    browserStatus.ShowSuccess("Loading: " + packName, 2.0f, UI::StatusMessage::DisplayMode::TimerWithFade);
    if (plugin_->cvarManager) plugin_->cvarManager->executeCommand("togglemenu settings");
}
```

## File: src/ui/TrainingPackUI.h
```c
#pragma once
#include "IMGUI/imgui.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "MapList.h"
#include "StatusMessageUI.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <cstring>

// Payload structure for dragging packs FROM bags (includes source bag info)
// This allows the drop target to know where the pack came from for removal
struct BagPackPayload
{
    char packCode[32];
    char sourceBag[32];
};

/*
 * ======================================================================================
 * TRAINING PACK UI: THE BROWSER WINDOW
 * ======================================================================================
 *
 * Two-panel floating browser window.
 *   Left  (60%): filterable/sortable selectable list with ImGuiListClipper
 *   Right (40%): selected-pack detail panel — thumbnail, metadata, action buttons
 *
 * Custom-pack creation lives in a modal popup opened from the left panel footer.
 */

class SuiteSpot;

class TrainingPackUI : public BakkesMod::Plugin::PluginWindow
{
  public:
    explicit TrainingPackUI(SuiteSpot* plugin);

    void Render() override;
    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    void SetImGuiContext(uintptr_t ctx) override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    void OnOpen() override;
    void OnClose() override;

    bool IsOpen();
    void SetOpen(bool open);

  private:
    SuiteSpot* plugin_;
    bool isWindowOpen_ = false;
    bool needsFocusOnNextRender_ = false;

    // ── Filter state ──────────────────────────────────────────────────
    char packSearchText[256] = {0};
    std::string packDifficultyFilter = "All";
    std::string packTagFilter;
    int packMinShots = 0;
    int packMaxShots = 50;
    bool packVideoFilter = false;
    int packSortColumn = 0;
    bool packSortAscending = true;

    // Cached last-seen values to detect changes without strcmp every frame
    char lastSearchText[256] = {0};
    std::string lastDifficultyFilter = "All";
    std::string lastTagFilter;
    int lastMinShots = 0;
    int lastMaxShots = 50;
    bool lastVideoFilter = false;
    int lastSortColumn = 0;
    bool lastSortAscending = true;

    // ── Filtered list cache ───────────────────────────────────────────
    std::vector<TrainingEntry> filteredPacks;
    bool packListInitialized = false;
    int lastPackCount = 0;

    // Tag dropdown population
    std::vector<std::string> availableTags;
    bool tagsInitialized = false;

    // ── Selection ─────────────────────────────────────────────────────
    std::string selectedPackCode;
    std::string lastQuickPicksSelected;

    // ── Thumbnail fetch ───────────────────────────────────────────────
    std::filesystem::path thumbnailCacheDir_;
    std::string lastFetchedThumbnailCode_;      // code we last triggered a fetch for
    std::shared_ptr<ImageWrapper> youtubeIcon_; // small YouTube logo for list indicator

    // Returns YouTube video ID from various URL formats, or "" if not a YouTube URL
    static std::string ExtractYouTubeId(const std::string& url);
    static std::string ExtractImgurId(const std::string& url);

    // Triggers async thumbnail download for the selected pack (idempotent)
    void FetchThumbnailForSelected();

    // ── Custom pack modal ────────────────────────────────────────────
    bool showAddPackModal_ = false;

    char customPackCode[20] = {0};
    char customPackName[128] = {0};
    char customPackCreator[64] = {0};
    int customPackDifficulty = 0;
    int customPackShotCount = 10;
    char customPackTags[256] = {0};
    char customPackNotes[512] = {0};
    char customPackVideoUrl[256] = {0};
    UI::StatusMessage customPackStatus;

    // ── Status ───────────────────────────────────────────────────────
    UI::StatusMessage browserStatus;

    // ── Helpers ──────────────────────────────────────────────────────
    void LoadPackImmediately(const std::string& packCode);
    void RenderDetailPanel(const TrainingEntry* pack);
    void RenderCustomPackModal();
    bool ValidatePackCode(const char* code) const;
    void ClearCustomPackForm();

    // Returns difficulty color for text/badges (same mapping as before)
    static ImVec4 DifficultyColor(const std::string& difficulty);
};
```

## File: src/utils/logging.h
```c
#pragma once
#include <string>
#include <source_location>
#include <format>
#include <memory>

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
constexpr bool DEBUG_LOG = false;


struct FormatString
{
	std::string_view str;
	std::source_location loc{};

	FormatString(const char* str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	FormatString(const std::string&& str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	[[nodiscard]] std::string GetLocation() const
	{
		return std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(), loc.line());
	}
};

struct FormatWstring
{
	std::wstring_view str;
	std::source_location loc{};

	FormatWstring(const wchar_t* str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	FormatWstring(const std::wstring&& str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	[[nodiscard]] std::wstring GetLocation() const
	{
		auto basic_string = std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(), loc.line());
		return std::wstring(basic_string.begin(), basic_string.end());
	}
};


template <typename... Args>
void LOG(std::string_view format_str, Args&&... args)
{
	_globalCvarManager->log(std::vformat(format_str, std::make_format_args(args...)));
}

template <typename... Args>
void LOG(std::wstring_view format_str, Args&&... args)
{
	_globalCvarManager->log(std::vformat(format_str, std::make_wformat_args(args...)));
}


template <typename... Args>
void DEBUGLOG(const FormatString& format_str, Args&&... args)
{
	if constexpr (DEBUG_LOG)
	{
		auto text = std::vformat(format_str.str, std::make_format_args(args...));
		auto location = format_str.GetLocation();
		_globalCvarManager->log(std::format("{} {}", text, location));
	}
}

template <typename... Args>
void DEBUGLOG(const FormatWstring& format_str, Args&&... args)
{
	if constexpr (DEBUG_LOG)
	{
		auto text = std::vformat(format_str.str, std::make_wformat_args(args...));
		auto location = format_str.GetLocation();
		_globalCvarManager->log(std::format(L"{} {}", text, location));
	}
}
```

## File: src/utils/ProcessUtils.h
```c
#pragma once
#include <string>
#include <Windows.h>

namespace Utils {

// Runs a command silently (no console window, no focus steal).
// Blocks until the process exits. Returns the process exit code, or -1 on failure.
inline int RunSilent(const std::string& executable, const std::string& args) {
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    std::string cmdLine = executable + " " + args;

    if (!CreateProcessA(
            executable.c_str(),
            const_cast<char*>(cmdLine.c_str()),
            NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
}

// Convenience: run a PowerShell command silently.
inline int RunPowerShell(const std::string& command) {
    std::string args = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + command + "\"";
    return RunSilent("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", args);
}

// Convenience: run a PowerShell script file silently with arguments.
inline int RunPowerShellScript(const std::string& scriptPath, const std::string& scriptArgs,
                               const std::string& logFile) {
    std::string args = "/c powershell.exe -NoProfile -ExecutionPolicy Bypass -File \""
                     + scriptPath + "\" " + scriptArgs + " > \"" + logFile + "\" 2>&1";
    return RunSilent("C:\\Windows\\System32\\cmd.exe", args);
}

// Convenience: expand a zip archive silently.
inline int ExpandArchive(const std::string& zipPath, const std::string& destPath) {
    std::string cmd = "try { Expand-Archive -LiteralPath '" + zipPath
                    + "' -DestinationPath '" + destPath + "' -Force; exit 0 } "
                    + "catch { Write-Error $_.Exception.Message; exit 1 }";
    return RunPowerShell(cmd);
}

} // namespace Utils
```

## File: src/core/TextureDownloader.cpp
```cpp
#include "pch.h"
#include "TextureDownloader.h"
#include "ProcessUtils.h"
#include "logging.h"
#include "bakkesmod/wrappers/http/HttpWrapper.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

TextureDownloader::TextureDownloader(std::shared_ptr<GameWrapper> gw, std::shared_ptr<CVarManagerWrapper> cm)
    : gameWrapper(gw), cvarManager(cm)
{
    bakkesModPath = gw->GetDataFolder().string() + "\\";
    FindCookedPCConsolePath();
}

void TextureDownloader::FindCookedPCConsolePath()
{
    std::string RLWin64_Path = std::filesystem::current_path().string();
    // RL runs from .../rocketleague/Binaries/Win64 (14 chars to strip)
    // We want .../rocketleague/TAGame/CookedPCConsole
    if (RLWin64_Path.length() > 14) {
        cookedPCConsolePath = RLWin64_Path.substr(0, RLWin64_Path.length() - 14) + "TAGame\\CookedPCConsole";
        LOG("CookedPCConsole path detected: {}", cookedPCConsolePath.string());
    } else {
        LOG("Error: Could not determine CookedPCConsole path from {}", RLWin64_Path);
    }
}

std::vector<std::string> TextureDownloader::CheckMissingTextures()
{
    std::vector<std::string> missingFiles;
    if (cookedPCConsolePath.empty()) return missingFiles;

    for (const auto& textureFile : WorkshopTexturesFilesList) {
        LOG("SuiteSpot [Textures]: Checking {}", textureFile);
        std::filesystem::path p = cookedPCConsolePath / textureFile;
        if (!std::filesystem::exists(p)) {
            LOG("SuiteSpot [Textures]: MISSING - {}", textureFile);
            missingFiles.push_back(textureFile);
        } else {
            LOG("SuiteSpot [Textures]: OK - {}", textureFile);
        }
    }
    return missingFiles;
}

void TextureDownloader::DownloadAndInstallTextures()
{
    if (isDownloading) return;
    isDownloading = true;
    downloadProgress = 0;
    fetchFailed = false;
    lastError.clear();

    std::string zipPath = bakkesModPath + "SuiteSpot\\Workshop\\Textures.zip";
    std::filesystem::create_directories(std::filesystem::path(zipPath).parent_path());

    LOG("SuiteSpot [Textures]: Fetching download URL from BakkesPlugins API...");
    FetchDownloadUrl(zipPath);
}

void TextureDownloader::FetchDownloadUrl(std::string zipPath)
{
    CurlRequest req;
    req.url = TEXTURES_API_URL;

    HttpWrapper::SendCurlRequest(req, [this, zipPath](int code, std::string responseText) {
        if (code != 200) {
            LOG("SuiteSpot [Textures]: API fetch failed (HTTP {})", code);
            lastError = "API fetch failed: HTTP " + std::to_string(code);
            fetchFailed = true;
            isDownloading = false;
            return;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(responseText);

            if (!j.contains("files") || !j["files"].is_array() || j["files"].empty()) {
                LOG("SuiteSpot [Textures]: API response missing files array");
                lastError = "API response invalid: no files array";
                fetchFailed = true;
                isDownloading = false;
                return;
            }

            auto& file = j["files"][0];
            std::string downloadUrl = file.value("edgeUrl", "");
            std::string expectedHash = file.value("fileHash", "");
            std::string version = file.value("versionString", "unknown");

            if (downloadUrl.empty()) {
                LOG("SuiteSpot [Textures]: API returned empty download URL");
                lastError = "API returned empty download URL";
                fetchFailed = true;
                isDownloading = false;
                return;
            }

            LOG("SuiteSpot [Textures]: Resolved v{} -> {}", version, downloadUrl);
            DownloadZip(zipPath, downloadUrl, expectedHash);

        } catch (const std::exception& e) {
            LOG("SuiteSpot [Textures]: Failed to parse API response: {}", e.what());
            lastError = std::string("API parse error: ") + e.what();
            fetchFailed = true;
            isDownloading = false;
        }
    });
}

void TextureDownloader::DownloadZip(std::string zipPath, std::string downloadUrl, std::string expectedHash)
{
    CurlRequest req;
    req.url = downloadUrl;
    req.progress_function = [this](double file_size, double downloaded, ...) {
        if (file_size > 0) {
            downloadProgress = (int)((downloaded / file_size) * 100.0);
        }
    };

    HttpWrapper::SendCurlRequest(req, [this, zipPath, expectedHash](int code, char* data, size_t size) {
        if (code != 200) {
            LOG("SuiteSpot [Textures]: Download failed (HTTP {})", code);
            lastError = "Download failed: HTTP " + std::to_string(code);
            fetchFailed = true;
            isDownloading = false;
            return;
        }

        // Verify SHA-256 hash before writing to disk
        if (!expectedHash.empty() && !VerifyHash(data, size, expectedHash)) {
            LOG("SuiteSpot [Textures]: Hash verification failed — file may be corrupted or tampered");
            lastError = "Hash verification failed";
            fetchFailed = true;
            isDownloading = false;
            return;
        }

        std::ofstream out_file(zipPath, std::ios::binary);
        if (!out_file) {
            LOG("SuiteSpot [Textures]: Failed to save zip to {}", zipPath);
            lastError = "Failed to write zip file";
            fetchFailed = true;
            isDownloading = false;
            return;
        }

        out_file.write(data, size);
        out_file.close();
        LOG("SuiteSpot [Textures]: Downloaded and verified. Extracting...");

        if (extractThread.joinable()) extractThread.join();
        extractThread = std::thread([this, zipPath]() {
            ExtractZip(zipPath, cookedPCConsolePath.string());
            LOG("SuiteSpot [Textures]: Installed successfully.");
            isDownloading = false;
            downloadProgress = 0;
        });
    });
}

bool TextureDownloader::VerifyHash(const char* data, size_t size, const std::string& expectedHex)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashLen = 0;
    DWORD cbData = 0;
    bool result = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        LOG("SuiteSpot [Textures]: BCryptOpenAlgorithmProvider failed");
        return false;
    }

    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(DWORD), &cbData, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    std::vector<BYTE> hash(hashLen);

    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0 &&
        BCryptHashData(hHash, (PUCHAR)data, (ULONG)size, 0) == 0 &&
        BCryptFinishHash(hHash, hash.data(), hashLen, 0) == 0) {
        std::ostringstream oss;
        for (DWORD i = 0; i < hashLen; ++i)
            oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

        std::string computed = oss.str();
        result = (computed == expectedHex);
        if (!result) LOG("SuiteSpot [Textures]: Hash mismatch — expected: {}, got: {}", expectedHex, computed);
    }

    if (hHash) BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

void TextureDownloader::ExtractZip(const std::string& zipPath, const std::string& destPath)
{
    int result = Utils::ExpandArchive(zipPath, destPath);
    if (result != 0) {
        LOG("SuiteSpot [Textures]: Failed to extract (exit code {})", result);
    }
}
```

## File: src/core/TextureDownloader.h
```c
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <atomic>
#include <thread>
#include "bakkesmod/plugin/bakkesmodplugin.h"

class TextureDownloader
{
  public:
    TextureDownloader(std::shared_ptr<GameWrapper> gw, std::shared_ptr<CVarManagerWrapper> cm);

    // The 14 .upk workshop editor texture files required for workshop maps to render
    const std::vector<std::string> WorkshopTexturesFilesList = {"EditorLandscapeResources.upk",
                                                                "EditorMaterials.upk",
                                                                "EditorMeshes.upk",
                                                                "EditorResources.upk",
                                                                "Engine_MI_Shaders.upk",
                                                                "EngineBuildings.upk",
                                                                "EngineDebugMaterials.upk",
                                                                "EngineMaterials.upk",
                                                                "EngineResources.upk",
                                                                "EngineVolumetrics.upk",
                                                                "MapTemplateIndex.upk",
                                                                "MapTemplates.upk",
                                                                "mods.upk",
                                                                "NodeBuddies.upk"};

    // BakkesPlugins API endpoint for the official workshop textures package (map ID 96).
    // Returns files[0].edgeUrl (CDN zip) and files[0].fileHash (SHA-256) for integrity verification.
    static constexpr const char* TEXTURES_API_URL = "https://bakkesplugins.com/api/rocket-league-maps/96";

    // Checks which textures are missing from CookedPCConsole. Returns empty vector if all present.
    std::vector<std::string> CheckMissingTextures();

    // Starts the two-step download: (1) fetch metadata from BakkesPlugins API to resolve the
    // CDN URL and expected SHA-256 hash, (2) download the ZIP and verify hash before extracting.
    void DownloadAndInstallTextures();

    // Status flags (read from UI)
    std::atomic<bool> isDownloading{false};
    std::atomic<int> downloadProgress{0};
    std::atomic<bool> fetchFailed{false};
    std::string lastError;

    bool dontAskAgain = false;

    ~TextureDownloader()
    {
        if (extractThread.joinable()) extractThread.join();
    }

  private:
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    std::filesystem::path cookedPCConsolePath;
    std::string bakkesModPath;
    std::thread extractThread;

    // Step 1: Query the BakkesPlugins API to get the resolved CDN URL and expected hash,
    // then call DownloadZip() with those values.
    void FetchDownloadUrl(std::string zipPath);

    // Step 2: Download the ZIP from the resolved CDN URL. Verifies SHA-256 hash before
    // writing to disk. Calls ExtractZip() on success.
    void DownloadZip(std::string zipPath, std::string downloadUrl, std::string expectedHash);

    // Verifies SHA-256 of data against expectedHex (uppercase hex string from API).
    // Uses Windows CNG (bcrypt.h) — no external dependency. Returns true if hash matches.
    bool VerifyHash(const char* data, size_t size, const std::string& expectedHex);

    void ExtractZip(const std::string& zipPath, const std::string& destPath);
    void FindCookedPCConsolePath();
};
```
