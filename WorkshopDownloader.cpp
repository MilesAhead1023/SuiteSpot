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
}

void WorkshopDownloader::GetResults(std::string keyWord, int IndexPage)
{
    // Prevent new search if one is already in progress
    bool expectedSearching = false;
    if (!RLMAPS_Searching.compare_exchange_strong(expectedSearching, true)) {
        LOG("Search already in progress, ignoring new search request");
        return;
    }

    // Reset state
    stopRequested = false;
    completedRequests = 0;
    RLMAPS_PageSelected = IndexPage;

    // Increment generation
    int currentGeneration = ++searchGeneration;

    // Clear list immediately under lock
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        RLMAPS_MapResultList.clear();
        listVersion++; // Notify UI to refresh
    }

    // Join previous thread if active
    if (searchThread.joinable()) {
        searchThread.join();
    }

    // Use weak_ptr to prevent crash if object is destroyed
    std::weak_ptr<WorkshopDownloader> weak_self = shared_from_this();

    // Run the entire search logic in a managed thread
    searchThread = std::thread([weak_self, keyWord, IndexPage, currentGeneration]() {
        auto self = weak_self.lock();
        if (!self) return;

        std::string searchUrl = self->rlmaps_url + keyWord + "&per_page=1000&page=" + std::to_string(IndexPage);

        CurlRequest req;
        req.url = searchUrl;

        // Pass weak_self to callback too
        HttpWrapper::SendCurlRequest(req, [weak_self, keyWord, currentGeneration](int code, std::string result) {
            auto self = weak_self.lock();
            if (!self) return;

            // Check cancellation
            if (self->stopRequested || self->searchGeneration != currentGeneration) {
                self->RLMAPS_Searching = false;
                return;
            }

            if (code != 200) {
                LOG("[ERR] Workshop search failed with HTTP code {}", code);
                self->RLMAPS_Searching = false;
                self->SearchErrorBool = true;
                self->SearchErrorText = "Search failed: HTTP " + std::to_string(code) + ". RLMAPS API may be down.";
                return;
            }

            // Clear previous search errors
            self->SearchErrorBool = false;
            self->SearchErrorText.clear();

            try {
                nlohmann::json actualJson = nlohmann::json::parse(result);

                if (!actualJson.is_array()) {
                    LOG("[ERR] Workshop search response is not an array");
                    self->RLMAPS_Searching = false;
                    self->SearchErrorBool = true;
                    self->SearchErrorText = "Invalid response from RLMAPS API (expected array)";
                    return;
                }

                self->RLMAPS_NumberOfMapsFound = (int)actualJson.size();
                LOG("Workshop search found {} maps", self->RLMAPS_NumberOfMapsFound.load());

                if (actualJson.empty()) {
                    self->RLMAPS_Searching = false;
                    return;
                }

                // 1. Populate the list with basic info immediately
                {
                    std::lock_guard<std::mutex> lock(self->resultsMutex);
                    LOG("Populating map list with {} items...", actualJson.size());
                    for (const auto& item : actualJson) {
                        if (!item.contains("id") || !item.contains("name")) continue;

                        RLMAPS_MapResult mapResult;

                        // Use safe JSON extraction with type checking
                        mapResult.ID = SafeGetString(item, "id", "");
                        mapResult.Name = SafeGetString(item, "name", "Unknown Map");
                        mapResult.Path = SafeGetString(item, "path", ""); // Store path for package URL construction
                        mapResult.Description = SafeGetString(item, "description", "");
                        if (!mapResult.Description.empty()) {
                            self->CleanHTML(mapResult.Description);
                        }
                        mapResult.Author = SafeGetNestedString(item, {"namespace", "path"}, "Unknown");

                        // We don't have preview URL yet
                        mapResult.PreviewUrl = "";

                        self->RLMAPS_MapResultList.push_back(mapResult);
                    }
                    self->listVersion++; // UI will render the list now (text only)
                    LOG("Map list populated. Version: {}", self->listVersion.load());
                }

                // Launch parallel threads for each map
                int totalMaps = actualJson.size();
                self->expectedResults = totalMaps;
                self->completedResults = 0;

                for (int i = 0; i < totalMaps; ++i) {
                    std::thread t(&WorkshopDownloader::FetchReleaseDetails, self.get(), i, currentGeneration);
                    t.detach();
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(50)); // 50ms delay between spawns (faster than before)
                }

                // Wait for all results to complete
                while (self->completedResults < self->expectedResults) {
                    if (self->stopRequested || self->searchGeneration != currentGeneration) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                self->RLMAPS_Searching = false;
                LOG("[OK] Workshop search complete: {}/{} maps loaded", self->completedResults.load(),
                    self->expectedResults.load());

            } catch (const std::exception& e) {
                LOG("[ERR] Workshop search JSON parse error: {}", e.what());
                self->RLMAPS_Searching = false;
                self->SearchErrorBool = true;
                self->SearchErrorText = "Failed to parse search results: " + std::string(e.what());
            }
        });
    });
}

void WorkshopDownloader::FetchReleaseDetails(int index, int generation)
{
    // Check cancellation or completion
    if (stopRequested || searchGeneration != generation) {
        completedResults++; // Must increment to prevent wait loop deadlock
        return;
    }

    std::string mapId;
    std::string mapName;
    size_t listSize = 0;

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        listSize = RLMAPS_MapResultList.size();
        if (index >= listSize) {
            // Done with all maps
            completedResults++; // Must increment to prevent wait loop deadlock
            return;
        }
        mapId = RLMAPS_MapResultList[index].ID;
        mapName = RLMAPS_MapResultList[index].Name;
    }

    std::string releaseUrl = "https://celab.jetfox.ovh/api/v4/projects/" + mapId + "/releases";

    LOG("📡 Fetching releases for '{}' (ID: {}) from: {}", mapName, mapId, releaseUrl);

    CurlRequest req;
    req.url = releaseUrl;

    std::weak_ptr<WorkshopDownloader> weak_self = shared_from_this();

    HttpWrapper::SendCurlRequest(req, [weak_self, index, generation, mapName, mapId](int code, std::string responseText) {
        auto self = weak_self.lock();

        if (!self) return;

        if (self->stopRequested || self->searchGeneration != generation) {

            LOG("FetchReleaseDetails cancelled for index {}", index);

            self->completedResults++; // Must increment to prevent wait loop deadlock
            return;
        }

        if (code == 200) {

            try {

                nlohmann::json releaseJson = nlohmann::json::parse(responseText);

                if (releaseJson.is_array() && !releaseJson.empty()) {

                    // ... (keep logic same)

                    std::vector<RLMAPS_Release> releases;

                    std::string previewUrl = "";

                    for (const auto& rItem : releaseJson) {

                        RLMAPS_Release release;

                        release.name = SafeGetString(rItem, "name", "Unknown Release");
                        release.tag_name = SafeGetString(rItem, "tag_name", "v1.0");
                        release.description = SafeGetString(rItem, "description", "");

                        // Parse assets by file extension instead of array index to handle API changes
                        if (rItem.contains("assets") && rItem["assets"].contains("links") &&
                            rItem["assets"]["links"].is_array()) {

                            std::string pictureLink;
                            std::string downloadLink;
                            std::string zipName;

                            for (const auto& link : rItem["assets"]["links"]) {
                                if (!link.contains("url") || !link.contains("name")) continue;

                                std::string url = link["url"].get<std::string>();
                                std::string name = link["name"].get<std::string>();
                                std::string nameLower = name;
                                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                                std::string linkType = SafeGetString(link, "link_type", "");

                                // Identify image links by link_type or extension
                                if (pictureLink.empty() &&
                                    (linkType == "image" || nameLower.ends_with(".jpg") ||
                                     nameLower.ends_with(".jpeg") || nameLower.ends_with(".png") ||
                                     nameLower.ends_with(".jfif") || nameLower.ends_with(".webp"))) {
                                    pictureLink = url;
                                }
                                // Identify download links by extension (link_type is "other" not "package")
                                else if (downloadLink.empty() && nameLower.ends_with(".zip")) {
                                    downloadLink = url;
                                    zipName = name;
                                }
                            }

                            if (!pictureLink.empty()) {
                                release.pictureLink = pictureLink;
                                if (previewUrl.empty()) previewUrl = pictureLink;
                            }

                            if (!downloadLink.empty()) {
                                release.downloadLink = downloadLink;

                                // Sanitize zip name
                                std::string zipNameSafe = zipName;
                                std::string specials[] = {"/", "\\", "?", ":", "*", "\"", "<", ">", "|", "#", "'"};
                                for (auto special : specials) {
                                    self->EraseAll(zipNameSafe, special);
                                }
                                release.zipName = zipNameSafe;
                            }
                        }

                        releases.push_back(release);
                    }

                    // Update map result with details

                    {

                        std::lock_guard<std::mutex> lock(self->resultsMutex);

                        // Verify generation, bounds, and ID match ALL inside lock
                        if (self->searchGeneration.load() != generation) {
                            LOG("FetchReleaseDetails: Stale generation {}, current {}", generation,
                                self->searchGeneration.load());
                            return;
                        }

                        if (index >= 0 && index < static_cast<int>(self->RLMAPS_MapResultList.size())) {
                            auto& mapResult = self->RLMAPS_MapResultList[index];

                            // Double-check ID hasn't changed
                            if (mapResult.ID == mapId) {
                                mapResult.releases = releases;
                                mapResult.PreviewUrl = previewUrl;
                                mapResult.Size = "10000000"; // Placeholder

                                // Check local cache for image
                                std::string imageExt = GetImageExtension(previewUrl);
                                mapResult.ImageExtension = imageExt; // Store for later use
                                fs::path resultImagePath = self->BakkesmodPath + "SuiteSpot\\Workshop\\img\\" + mapId +
                                                           imageExt;

                                if (!self->DirectoryOrFileExists(resultImagePath)) {

                                    mapResult.IsDownloadingPreview = true;

                                    self->DownloadPreviewImage(previewUrl, resultImagePath.string(), index, generation);

                                } else {

                                    mapResult.ImagePath = resultImagePath;

                                    mapResult.isImageLoaded = true;
                                }

                                self->listVersion++; // Update UI with new details

                                LOG("Details loaded for map {}, version: {}", index, self->listVersion.load());

                            } else {
                                LOG("FetchReleaseDetails: Map ID mismatch at index {} (expected {}, got {})", index,
                                    mapId, mapResult.ID);
                            }
                        } else {
                            LOG("FetchReleaseDetails: Index {} out of bounds (size: {})", index,
                                self->RLMAPS_MapResultList.size());
                        }
                    }
                }

            } catch (...) {

                LOG("Failed to parse releases for map {}", mapName);
            }

        } else {

            LOG("Failed to fetch releases for map {} (code {})", mapName, code);
        }

        // Increment completed counter (Vync's approach - no recursive calls)
        self->completedResults++;
    });
}

void WorkshopDownloader::FetchImageOnly(int index, int generation)
{
    // Lightweight image fetch - uses /packages endpoint instead of /releases
    // This is much faster as we only need package name and version to construct image URL

    if (stopRequested || searchGeneration != generation) {
        completedResults++;
        return;
    }

    std::string mapId;
    std::string mapPath;
    std::string mapName;

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        if (index >= static_cast<int>(RLMAPS_MapResultList.size())) {
            completedResults++;
            return;
        }
        mapId = RLMAPS_MapResultList[index].ID;
        mapPath = RLMAPS_MapResultList[index].Path;
        mapName = RLMAPS_MapResultList[index].Name;
    }

    // If no path, we can't construct the URL
    if (mapPath.empty()) {
        LOG("FetchImageOnly: No path for map {} (ID: {})", mapName, mapId);
        completedResults++;
        return;
    }

    std::string packagesUrl = "https://celab.jetfox.ovh/api/v4/projects/" + mapId + "/packages";
    LOG("🔗 Fetching packages for '{}' (ID: {}) from: {}", mapName, mapId, packagesUrl);

    CurlRequest req;
    req.url = packagesUrl;

    std::weak_ptr<WorkshopDownloader> weak_self = shared_from_this();

    HttpWrapper::SendCurlRequest(req, [weak_self, index, generation, mapName, mapId,
                                       mapPath](int code, std::string responseText) {
        auto self = weak_self.lock();
        if (!self) return;

        if (self->stopRequested || self->searchGeneration != generation) {
            self->completedResults++;
            return;
        }

        if (code == 200) {
            try {
                nlohmann::json packagesJson = nlohmann::json::parse(responseText);

                if (packagesJson.is_array() && !packagesJson.empty()) {
                    // Get the latest package (last in array)
                    auto& latestPackage = packagesJson.back();

                    std::string packageName = latestPackage.contains("name") ? latestPackage["name"].get<std::string>()
                                                                             : mapPath;
                    std::string packageVersion = latestPackage.contains("version")
                                                     ? latestPackage["version"].get<std::string>()
                                                     : "v1.0";

                    // Construct image URL: /projects/{id}/packages/generic/{name}/{version}/{name}.jpg
                    std::string previewUrl = "https://celab.jetfox.ovh/api/v4/projects/" + mapId + "/packages/generic/" +
                                             packageName + "/" + packageVersion + "/" + packageName + ".jpg";

                    LOG("Constructed image URL for '{}': {}", mapName, previewUrl);

                    // Update map result with preview URL
                    {
                        std::lock_guard<std::mutex> lock(self->resultsMutex);

                        if (self->searchGeneration.load() != generation) {
                            self->completedResults++;
                            return;
                        }

                        if (index >= 0 && index < static_cast<int>(self->RLMAPS_MapResultList.size())) {
                            auto& mapResult = self->RLMAPS_MapResultList[index];

                            if (mapResult.ID == mapId) {
                                mapResult.PreviewUrl = previewUrl;
                                mapResult.ImageExtension = ".jpg";

                                // Check local cache for image
                                fs::path resultImagePath = self->BakkesmodPath + "SuiteSpot\\Workshop\\img\\" + mapId +
                                                           ".jpg";

                                if (self->DirectoryOrFileExists(resultImagePath)) {
                                    mapResult.ImagePath = resultImagePath;
                                    mapResult.isImageLoaded = true;
                                    LOG("Image already cached for '{}': {}", mapName, resultImagePath.string());
                                } else {
                                    mapResult.IsDownloadingPreview = true;
                                    self->DownloadPreviewImage(previewUrl, resultImagePath.string(), index, generation);
                                }

                                self->listVersion++;
                            }
                        }
                    }
                } else {
                    LOG("No packages found for '{}' (ID: {})", mapName, mapId);
                }
            } catch (const std::exception& e) {
                LOG("Failed to parse packages for '{}': {}", mapName, e.what());
            }
        } else {
            LOG("Failed to fetch packages for '{}' (code {})", mapName, code);
        }

        self->completedResults++;
    });
}

void WorkshopDownloader::GetNumPages(std::string keyWord)
{
    int ResultsSize = 1000;
    std::string searchUrl = rlmaps_url + keyWord + "&per_page=1000";

    CurlRequest req;
    req.url = searchUrl;

    HttpWrapper::SendCurlRequest(req, [this, ResultsSize](int code, std::string result) {
        if (code != 200) return;

        try {
            nlohmann::json actualJson = nlohmann::json::parse(result);
            if (actualJson.is_array()) {
                NumPages = ((int)actualJson.size() / ResultsSize) + 1;
                LOG("Workshop search found {} pages", NumPages.load());
            }
        } catch (...) {
            // Ignore parse errors for page count
        }
    });
}

void WorkshopDownloader::StopSearch()
{
    stopRequested = true;
    searchGeneration++; // Invalidate any pending callbacks

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        RLMAPS_MapResultList.clear();
        RLMAPS_NumberOfMapsFound = 0;
        listVersion++;
    }

    resultsCV.notify_all(); // Wake up the waiter
    RLMAPS_Searching = false;
    LOG("Search stop requested and list cleared.");
}

void WorkshopDownloader::RLMAPS_DownloadWorkshop(std::string folderpath, RLMAPS_MapResult mapResult,
                                                 RLMAPS_Release release)
{
    // Donor plugin pattern: spin-wait for user confirmation via UI
    UserIsChoosingYESorNO = true;

    while (UserIsChoosingYESorNO) {
        Sleep(100);
    }

    if (!AcceptTheDownload) {
        return;
    }

    std::string workshopSafeMapName = SanitizeMapName(mapResult.Name);

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

    CreateJSONLocalWorkshopInfos(workshopSafeMapName, Workshop_Dl_Path + "/", mapResult.Name, mapResult.Author,
                                 mapResult.Description, mapResult.PreviewUrl);
    LOG("JSON created: {}/{}.json", Workshop_Dl_Path, workshopSafeMapName);

    if (DirectoryOrFileExists(mapResult.ImagePath)) {
        std::string imageExt = mapResult.ImageExtension.empty() ? ".jfif" : mapResult.ImageExtension;
        fs::copy(mapResult.ImagePath, Workshop_Dl_Path + "/" + workshopSafeMapName + imageExt);
        LOG("Preview pasted: {}/{}{}", Workshop_Dl_Path, workshopSafeMapName, imageExt);
    }

    std::string download_url = release.downloadLink;
    LOG("Download URL: {}", download_url);
    std::string Folder_Path = Workshop_Dl_Path + "/" + release.zipName;

    RLMAPS_WorkshopDownload_Progress = 0;
    RLMAPS_Download_Progress = 0;
    RLMAPS_IsDownloadingWorkshop = true;

    LOG("Download starting...");

    CurlRequest req;
    req.url = download_url;
    req.progress_function = [this](double file_size, double downloaded, ...) {
        RLMAPS_Download_Progress = downloaded;
        RLMAPS_WorkshopDownload_FileSize = file_size;
    };

    HttpWrapper::SendCurlRequest(req, [this, Folder_Path, Workshop_Dl_Path](int code, char* data, size_t size) {
        if (code == 200) {
            std::ofstream out_file{Folder_Path, std::ios_base::binary};
            if (out_file) {
                out_file.write(data, size);
                out_file.close();
                LOG("Workshop downloaded to: {}", Workshop_Dl_Path);
                RLMAPS_IsDownloadingWorkshop = false;
            }
        } else {
            LOG("Workshop download failed with code {}", code);
            RLMAPS_IsDownloadingWorkshop = false;
        }
    });

    // Spin-wait for download to complete (donor plugin pattern)
    while (RLMAPS_IsDownloadingWorkshop) {
        RLMAPS_WorkshopDownload_Progress = RLMAPS_Download_Progress.load();
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
                            mapResultIndex < self->RLMAPS_MapResultList.size()) {
                            self->RLMAPS_MapResultList[mapResultIndex].ImagePath = filePath;
                            // Image loading deferred to Render thread to avoid crash
                            self->RLMAPS_MapResultList[mapResultIndex].IsDownloadingPreview = false;
                            self->listVersion++; // Notify UI
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
