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
