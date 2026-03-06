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
