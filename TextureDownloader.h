#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <atomic>
#include <thread>
#include "bakkesmod/plugin/bakkesmodplugin.h"

class TextureDownloader {
public:
    TextureDownloader(std::shared_ptr<GameWrapper> gw, std::shared_ptr<CVarManagerWrapper> cm);

    // List of texture files to check for
    const std::vector<std::string> WorkshopTexturesFilesList = {
        "EditorLandscapeResources.upk", "EditorMaterials.upk", "EditorMeshes.upk", 
        "EditorResources.upk", "Engine_MI_Shaders.upk", "EngineBuildings.upk", 
        "EngineDebugMaterials.upk", "EngineMaterials.upk", "EngineResources.upk", 
        "EngineVolumetrics.upk", "MapTemplateIndex.upk", "MapTemplates.upk", 
        "mods.upk", "NodeBuddies.upk"
    };

    // Checks if textures are present in the CookedPCConsole folder
    std::vector<std::string> CheckMissingTextures();

    // Starts the download and installation process
    void DownloadAndInstallTextures();

    // Status flags
    std::atomic<bool> isDownloading{false};
    std::atomic<int> downloadProgress{0};
    
    // Config
    bool dontAskAgain = false;

    ~TextureDownloader() {
        if (extractThread.joinable()) extractThread.join();
    }

private:
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    std::filesystem::path cookedPCConsolePath;
    std::string bakkesModPath;
    std::thread extractThread;

    // Helper to extract zip files
    void ExtractZip(const std::string& zipPath, const std::string& destPath);
    
    // Helper to find CookedPCConsole path
    void FindCookedPCConsolePath();
};
