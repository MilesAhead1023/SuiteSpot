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
