#include "pch.h"
#include "TextureDownloader.h"
#include "ProcessUtils.h"
#include "logging.h"
#include "bakkesmod/wrappers/http/HttpWrapper.h"
#include <fstream>

TextureDownloader::TextureDownloader(std::shared_ptr<GameWrapper> gw, std::shared_ptr<CVarManagerWrapper> cm) 
    : gameWrapper(gw), cvarManager(cm)
{
    bakkesModPath = gw->GetDataFolder().string() + "\\";
    FindCookedPCConsolePath();
}

void TextureDownloader::FindCookedPCConsolePath() {
    // Logic from original plugin
    std::string RLWin64_Path = std::filesystem::current_path().string();
    // RL is usually in .../rocketleague/Binaries/Win64
    // We want .../rocketleague/TAGame/CookedPCConsole
    
    // Check if we are in Win64 - rudimentary check
    // Ideally we'd look for "Binaries\\Win64"
    
    // Original logic: RLCookedPCConsole_Path = RLWin64_Path.substr(0, RLWin64_Path.length() - 14) + "TAGame\\CookedPCConsole";
    // 14 chars is "\Binaries\Win64" (backslashes) or "/Binaries/Win64"
    
    if (RLWin64_Path.length() > 14) {
         cookedPCConsolePath = RLWin64_Path.substr(0, RLWin64_Path.length() - 14) + "TAGame\\CookedPCConsole";
         LOG("CookedPCConsole path detected: {}", cookedPCConsolePath.string());
    } else {
        // Fallback or error
        LOG("Error: Could not determine CookedPCConsole path from {}", RLWin64_Path);
    }
}

std::vector<std::string> TextureDownloader::CheckMissingTextures() {
    std::vector<std::string> missingFiles;
    if (cookedPCConsolePath.empty()) return missingFiles;

    for (const auto& textureFile : WorkshopTexturesFilesList) {
        std::filesystem::path p = cookedPCConsolePath / textureFile;
        if (!std::filesystem::exists(p)) {
            missingFiles.push_back(textureFile);
        }
    }
    return missingFiles;
}

void TextureDownloader::DownloadAndInstallTextures() {
    if (isDownloading) return;
    isDownloading = true;
    downloadProgress = 0;

    std::string zipPath = bakkesModPath + "SuiteSpot\\Workshop\\Textures.zip";
    
    // Ensure directory exists
    std::filesystem::create_directories(std::filesystem::path(zipPath).parent_path());

    LOG("Starting texture download to {}", zipPath);

    CurlRequest req;
    req.url = "https://drive.usercontent.google.com/download?id=1jklpjfEu4Yw97cjYaMDWRx8H2XFyji6U&export=download&confirm=t";
    req.progress_function = [this](double file_size, double downloaded, ...) {
        if (file_size > 0) {
            downloadProgress = (int)((downloaded / file_size) * 100.0);
        }
    };

    HttpWrapper::SendCurlRequest(req, [this, zipPath](int code, char* data, size_t size) {
        if (code == 200) {
            std::ofstream out_file(zipPath, std::ios::binary);
            if (out_file) {
                out_file.write(data, size);
                out_file.close();
                LOG("Textures downloaded. Extracting...");
                
                // Run extraction in a separate thread to avoid freezing the game
                if (extractThread.joinable()) extractThread.join();
                extractThread = std::thread([this, zipPath]() {
                    ExtractZip(zipPath, cookedPCConsolePath.string());
                    LOG("Textures installed successfully.");
                    isDownloading = false;
                    downloadProgress = 0;
                });
            } else {
                LOG("Failed to save texture zip.");
                isDownloading = false;
                downloadProgress = 0;
            }
        } else {
            LOG("Texture download failed with code {}", code);
            isDownloading = false;
            downloadProgress = 0;
        }
    });
}

void TextureDownloader::ExtractZip(const std::string& zipPath, const std::string& destPath) {
    int result = Utils::ExpandArchive(zipPath, destPath);
    if (result != 0) {
        LOG("Failed to extract textures (exit code {})", result);
    }
}
