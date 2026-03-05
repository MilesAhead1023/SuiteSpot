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
