# SuiteSpot Training Pack Updater
# Downloads training packs from online source and exports as JSON
#
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File SuitePackGrabber.ps1 -OutputPath "C:\path\to\output.json"
# Default output: %APPDATA%\bakkesmod\bakkesmod\data\SuiteSpot\TrainingSuite\training_packs.json
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
