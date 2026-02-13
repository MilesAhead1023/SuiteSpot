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

struct RLMAPS_Release
{
    std::string name;
    std::string tag_name;
    std::string description;
    std::string zipName;
    std::string downloadLink;
    std::string pictureLink;
};

struct RLMAPS_MapResult
{
    std::string ID;
    std::string Name;
    std::string Path;  // Project path for constructing package URLs
    std::string Size;
    std::string Description;
    std::string PreviewUrl;
    std::string Author;
    std::vector<RLMAPS_Release> releases;
    fs::path ImagePath;
    std::string ImageExtension;  // Store detected image extension (.jpg, .png, .webp, etc.)
    std::shared_ptr<ImageWrapper> Image;
    bool isImageLoaded = false;
    bool IsDownloadingPreview = false;
};

class WorkshopDownloader : public std::enable_shared_from_this<WorkshopDownloader>
{
public:
    WorkshopDownloader(std::shared_ptr<GameWrapper> gw);
    ~WorkshopDownloader();
    
    void GetResults(std::string keyWord, int IndexPage);
    void FetchReleaseDetails(int index, int generation);
    void FetchImageOnly(int index, int generation);  // Lightweight: only fetches image via /packages endpoint

    void GetNumPages(std::string keyWord);
    
    void RLMAPS_DownloadWorkshop(std::string folderpath, RLMAPS_MapResult mapResult, RLMAPS_Release release);
    void DownloadPreviewImage(std::string downloadUrl, std::string filePath, int mapResultIndex, int generation);
    
    void CreateJSONLocalWorkshopInfos(std::string jsonFileName, std::string workshopMapPath, 
                                     std::string mapTitle, std::string mapAuthor, 
                                     std::string mapDescription, std::string mapPreviewUrl);
    int ExtractZipPowerShell(std::string zipFilePath, std::string destinationPath);
    void RenameFileToUPK(fs::path filePath);
    std::string UdkInDirectory(std::string dirPath);
    
    // Cancels any active search
    void StopSearch();

    std::atomic<bool> RLMAPS_Searching = false;
    std::atomic<int> RLMAPS_NumberOfMapsFound = 0;
    std::atomic<int> NumPages = 0;
    std::atomic<int> RLMAPS_PageSelected = 0;
    std::vector<RLMAPS_MapResult> RLMAPS_MapResultList;
    
    std::atomic<bool> RLMAPS_IsDownloadingWorkshop = false;
    std::atomic<int> RLMAPS_Download_Progress = 0;
    std::atomic<int> RLMAPS_WorkshopDownload_Progress = 0;
    std::atomic<int> RLMAPS_WorkshopDownload_FileSize = 0;
    
    // Download confirmation flags (donor plugin pattern: thread spin-waits for UI)
    std::atomic<bool> UserIsChoosingYESorNO = false;
    std::atomic<bool> AcceptTheDownload = false;
    
    std::atomic<bool> FolderErrorBool = false;
    std::string FolderErrorText;

    std::atomic<bool> SearchErrorBool = false;
    std::string SearchErrorText;
    
    std::string BakkesmodPath;
    std::string IfNoPreviewImagePath;
    std::string rlmaps_url = "https://celab.jetfox.ovh/api/v4/projects/?search=";

    mutable std::mutex resultsMutex; // Protects RLMAPS_MapResultList
    std::condition_variable resultsCV; // Signals when map results are ready
    std::atomic<int> completedRequests = 0;
    std::atomic<int> completedResults = 0;  // Tracks completed release detail fetches
    std::atomic<int> expectedResults = 0;   // Total number of maps to fetch
    std::atomic<int> searchGeneration = 0;
    std::atomic<bool> stopRequested = false; // Flag to abort the search loop
    std::atomic<int> listVersion = 0; // Incremented whenever the list is modified, so UI knows when to copy

    // Helper to get current generation safely
    int GetSearchGeneration() const { return searchGeneration.load(); }
    
    std::string SanitizeMapName(const std::string& name);

private:
    std::shared_ptr<GameWrapper> gameWrapper;
    std::thread searchThread; // Worker thread for search operations
    
    void CleanHTML(std::string& S);
    void EraseAll(std::string& str, const std::string& from);
    void ReplaceAll(std::string& str, const std::string& from, const std::string& to);
    bool DirectoryOrFileExists(const fs::path& p);
};
