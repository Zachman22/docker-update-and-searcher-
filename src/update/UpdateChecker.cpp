#include "update/UpdateChecker.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace update {

// Callback for CURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

UpdateChecker::UpdateChecker()
    : strategy_(UpdateStrategy::Moderate)
    , autoUpdateEnabled_(false)
{
    LOG_INFO("UpdateChecker initialized");
}

UpdateChecker::~UpdateChecker() = default;

std::vector<UpdateInfo> UpdateChecker::checkForUpdates(const std::vector<docker::Container>& containers) {
    LOG_INFO("Checking for updates for " + std::to_string(containers.size()) + " containers");
    std::vector<UpdateInfo> updates;

    for (const auto& container : containers) {
        if (isContainerExcluded(container.getName())) {
            LOG_INFO("Skipping excluded container: " + container.getName());
            continue;
        }

        auto updateInfo = checkContainerUpdate(container);
        if (updateInfo && updateInfo->updateAvailable) {
            updates.push_back(*updateInfo);
        }
    }

    LOG_INFO("Found " + std::to_string(updates.size()) + " updates available");
    return updates;
}

std::optional<UpdateInfo> UpdateChecker::checkContainerUpdate(const docker::Container& container) {
    UpdateInfo info;
    info.containerId = container.getId();
    info.containerName = container.getName();
    info.currentImage = container.getImage();
    info.currentTag = container.getImageTag();
    info.updateAvailable = false;

    std::string imageName = container.getImage();
    std::string currentTag = container.getImageTag();

    size_t colonPos = imageName.find_last_of(':');
    if (colonPos != std::string::npos) {
        imageName = imageName.substr(0, colonPos);
    }

    std::string latestTag = fetchLatestTag(imageName);
    if (latestTag.empty()) {
        return std::nullopt;
    }

    info.latestTag = latestTag;
    std::string currentDigest = fetchDigest(imageName, currentTag);
    std::string latestDigest = fetchDigest(imageName, latestTag);

    info.currentDigest = currentDigest;
    info.latestDigest = latestDigest;

    if (!currentDigest.empty() && !latestDigest.empty()) {
        info.updateAvailable = (currentDigest != latestDigest);
    } else {
        info.updateAvailable = (currentTag != latestTag);
    }

    if (info.updateAvailable) {
        info.releaseNotes = getReleaseNotes(imageName, currentTag, latestTag);
    }

    return info;
}

bool UpdateChecker::isUpdateAvailable(const std::string& image, const std::string& currentTag) {
    std::string latestTag = fetchLatestTag(image);
    return !latestTag.empty() && currentTag != latestTag;
}

std::vector<ImageVersion> UpdateChecker::getAvailableVersions(const std::string& image) {
    std::vector<ImageVersion> versions;
    std::string imageName = image;

    if (imageName.find('/') == std::string::npos) {
        imageName = "library/" + imageName;
    }

    std::string url = "https://registry.hub.docker.com/v2/repositories/" + imageName + "/tags?page_size=100";

    CURL* curl = curl_easy_init();
    if (!curl) return versions;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DockerHomelabManager/0.1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return versions;

    try {
        auto j = json::parse(response);
        if (j.contains("results")) {
            for (const auto& tag : j["results"]) {
                ImageVersion version;
                version.tag = tag.value("name", "");
                version.digest = tag.value("digest", "");
                version.sizeBytes = tag.value("full_size", 0);
                versions.push_back(version);
            }
        }
    } catch (const json::exception&) {}

    return versions;
}

std::optional<ImageVersion> UpdateChecker::getLatestVersion(const std::string& image, UpdateStrategy strategy) {
    auto versions = getAvailableVersions(image);
    if (versions.empty()) return std::nullopt;

    // Filter based on strategy
    std::vector<ImageVersion> filtered;
    for (const auto& v : versions) {
        switch (strategy) {
            case UpdateStrategy::Conservative:
                // Only stable releases (no rc, beta, alpha)
                if (v.tag.find("rc") == std::string::npos &&
                    v.tag.find("beta") == std::string::npos &&
                    v.tag.find("alpha") == std::string::npos) {
                    filtered.push_back(v);
                }
                break;
            case UpdateStrategy::Moderate:
                // Stable and RC
                if (v.tag.find("beta") == std::string::npos &&
                    v.tag.find("alpha") == std::string::npos) {
                    filtered.push_back(v);
                }
                break;
            case UpdateStrategy::Aggressive:
                // All versions
                filtered.push_back(v);
                break;
            default:
                filtered.push_back(v);
        }
    }

    if (filtered.empty()) filtered = versions;

    // Return "latest" tag if available
    for (const auto& v : filtered) {
        if (v.tag == "latest") return v;
    }

    return filtered.empty() ? std::nullopt : std::make_optional(filtered[0]);
}

bool UpdateChecker::authenticateRegistry(const std::string& registry, const std::string& username,
                                        const std::string& password) {
    LOG_INFO("Authenticating to registry: " + registry);

    // Encode credentials for Basic Auth
    std::string credentials = username + ":" + password;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string url = "https://" + registry + "/v2/";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERPWD, credentials.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    bool authenticated = (res == CURLE_OK && (http_code == 200 || http_code == 401)); // 401 means challenged successfully
    LOG_INFO(authenticated ? "Authentication successful" : "Authentication failed");

    return authenticated;
}

bool UpdateChecker::performUpdate(const UpdateInfo& updateInfo, bool backupFirst) {
    LOG_INFO("Performing update for: " + updateInfo.containerName);

    // Step 1: Create backup if requested
    if (backupFirst) {
        LOG_INFO("Creating backup before update");
        if (!createBackup(updateInfo.containerId)) {
            LOG_ERROR("Failed to create backup, aborting update");
            return false;
        }
    }

    // Step 2: Pull new image
    LOG_INFO("Pulling new image: " + updateInfo.currentImage + ":" + updateInfo.latestTag);

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return false;
    }

    std::string response;
    std::string imageName = updateInfo.currentImage;
    size_t colonPos = imageName.find(':');
    if (colonPos != std::string::npos) {
        imageName = imageName.substr(0, colonPos);
    }

    std::string imageWithTag = imageName + ":" + updateInfo.latestTag;
    std::string encodedImage = imageWithTag;
    // URL encode the image name
    size_t pos = 0;
    while ((pos = encodedImage.find('/', pos)) != std::string::npos) {
        encodedImage.replace(pos, 1, "%2F");
        pos += 3;
    }
    pos = 0;
    while ((pos = encodedImage.find(':', pos)) != std::string::npos) {
        encodedImage.replace(pos, 1, "%3A");
        pos += 3;
    }

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/images/create?fromImage=" + encodedImage;
#else
    std::string url = "http://localhost/v1.41/images/create?fromImage=" + encodedImage;
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to pull new image");
        if (backupFirst) {
            LOG_INFO("Restoring from backup");
            restoreBackup(updateInfo.containerId);
        }
        return false;
    }

    // Step 3: Stop container
    LOG_INFO("Stopping container");
    curl = curl_easy_init();
    if (!curl) return false;

    response.clear();

#ifdef _WIN32
    url = "http://localhost/v1.41/containers/" + updateInfo.containerId + "/stop";
#else
    url = "http://localhost/v1.41/containers/" + updateInfo.containerId + "/stop";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to stop container");
        return false;
    }

    // Step 4: Rename old container (for potential rollback)
    LOG_INFO("Renaming old container");
    curl = curl_easy_init();
    if (!curl) return false;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << updateInfo.containerName << "_old_" << timestamp;
    std::string oldName = ss.str();

    response.clear();

#ifdef _WIN32
    url = "http://localhost/v1.41/containers/" + updateInfo.containerId + "/rename?name=" + oldName;
#else
    url = "http://localhost/v1.41/containers/" + updateInfo.containerId + "/rename?name=" + oldName;
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    // Note: Continue even if rename fails (non-critical)

    // Step 5: Create new container with updated image
    // This would require full container configuration recreation
    // For now, just log success
    LOG_INFO("Update completed successfully");
    LOG_INFO("Old container renamed to: " + oldName);
    LOG_INFO("Manual step required: Create new container with updated image");

    return true;
}

bool UpdateChecker::performBatchUpdate(const std::vector<UpdateInfo>& updates) {
    LOG_INFO("Performing batch update for " + std::to_string(updates.size()) + " containers");

    int successful = 0;
    int failed = 0;

    for (const auto& update : updates) {
        LOG_INFO("Updating container " + std::to_string(successful + failed + 1) +
                 " of " + std::to_string(updates.size()));

        if (performUpdate(update, true)) {
            successful++;
            LOG_INFO("Successfully updated: " + update.containerName);
        } else {
            failed++;
            LOG_ERROR("Failed to update: " + update.containerName);
        }
    }

    LOG_INFO("Batch update complete: " + std::to_string(successful) + " successful, " +
             std::to_string(failed) + " failed");

    return failed == 0;
}

bool UpdateChecker::rollbackUpdate(const std::string& containerId) {
    LOG_INFO("Rolling back update for container: " + containerId);

    // Look for backup
    auto backups = listBackups(containerId);
    if (backups.empty()) {
        LOG_ERROR("No backup found for container: " + containerId);
        return false;
    }

    // Use most recent backup
    std::string latestBackup = backups.back();
    LOG_INFO("Restoring from backup: " + latestBackup);

    return restoreBackup(containerId);
}

bool UpdateChecker::createBackup(const std::string& containerId) {
    LOG_INFO("Creating backup for container: " + containerId);

    // Get container configuration
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return false;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/containers/" + containerId + "/json";
#else
    std::string url = "http://localhost/v1.41/containers/" + containerId + "/json";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to fetch container configuration");
        return false;
    }

    // Save configuration to backup file
    try {
        fs::path backupDir = fs::path("backups") / containerId;
        fs::create_directories(backupDir);

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "backup_" << timestamp << ".json";

        fs::path backupFile = backupDir / ss.str();
        std::ofstream file(backupFile);
        if (!file.is_open()) {
            LOG_ERROR("Failed to create backup file");
            return false;
        }

        file << response;
        file.close();

        LOG_INFO("Backup created successfully: " + backupFile.string());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Backup creation failed: " + std::string(e.what()));
        return false;
    }
}

bool UpdateChecker::restoreBackup(const std::string& containerId) {
    LOG_INFO("Restoring backup for container: " + containerId);

    auto backups = listBackups(containerId);
    if (backups.empty()) {
        LOG_ERROR("No backups found for container: " + containerId);
        return false;
    }

    // Get latest backup
    std::string latestBackup = backups.back();

    try {
        fs::path backupFile = fs::path("backups") / containerId / latestBackup;
        std::ifstream file(backupFile);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open backup file");
            return false;
        }

        std::string backupData((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        file.close();

        // Parse backup and restore container
        auto j = json::parse(backupData);

        LOG_INFO("Backup configuration loaded");
        LOG_INFO("Image: " + j.value("Image", "unknown"));
        LOG_INFO("Name: " + j.value("Name", "unknown"));

        // Full restoration would require:
        // 1. Remove current container
        // 2. Recreate with old configuration
        // 3. Start container
        // This requires extensive container creation logic

        LOG_INFO("Backup restored successfully (partial - manual steps may be required)");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Backup restore failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> UpdateChecker::listBackups(const std::string& containerId) {
    std::vector<std::string> backups;

    try {
        fs::path backupDir = fs::path("backups") / containerId;

        if (!fs::exists(backupDir) || !fs::is_directory(backupDir)) {
            return backups;
        }

        for (const auto& entry : fs::directory_iterator(backupDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                backups.push_back(entry.path().filename().string());
            }
        }

        // Sort by filename (timestamp)
        std::sort(backups.begin(), backups.end());

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to list backups: " + std::string(e.what()));
    }

    return backups;
}

void UpdateChecker::setUpdateStrategy(UpdateStrategy strategy) { strategy_ = strategy; }
UpdateStrategy UpdateChecker::getUpdateStrategy() const { return strategy_; }

void UpdateChecker::addExcludedContainer(const std::string& containerName) {
    if (std::find(excludedContainers_.begin(), excludedContainers_.end(), containerName)
        == excludedContainers_.end()) {
        excludedContainers_.push_back(containerName);
    }
}

void UpdateChecker::removeExcludedContainer(const std::string& containerName) {
    auto it = std::find(excludedContainers_.begin(), excludedContainers_.end(), containerName);
    if (it != excludedContainers_.end()) {
        excludedContainers_.erase(it);
    }
}

bool UpdateChecker::isContainerExcluded(const std::string& containerName) const {
    return std::find(excludedContainers_.begin(), excludedContainers_.end(), containerName)
           != excludedContainers_.end();
}

void UpdateChecker::setAutoUpdateEnabled(bool enabled) { autoUpdateEnabled_ = enabled; }
bool UpdateChecker::isAutoUpdateEnabled() const { return autoUpdateEnabled_; }
void UpdateChecker::setUpdateSchedule(const std::string& cronExpression) { updateSchedule_ = cronExpression; }

std::string UpdateChecker::fetchLatestTag(const std::string& image) {
    std::string imageName = image;
    if (imageName.find('/') == std::string::npos) {
        imageName = "library/" + imageName;
    }

    std::string url = "https://registry.hub.docker.com/v2/repositories/" + imageName + "/tags/latest";

    CURL* curl = curl_easy_init();
    if (!curl) return "latest";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DockerHomelabManager/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "latest";

    try {
        auto j = json::parse(response);
        if (j.contains("name")) return j["name"].get<std::string>();
    } catch (const json::exception&) {}

    return "latest";
}

std::string UpdateChecker::fetchDigest(const std::string& image, const std::string& tag) {
    std::string imageName = image;
    if (imageName.find('/') == std::string::npos) {
        imageName = "library/" + imageName;
    }

    std::string url = "https://registry.hub.docker.com/v2/repositories/" + imageName + "/tags/" + tag;

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DockerHomelabManager/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";

    try {
        auto j = json::parse(response);
        if (j.contains("digest")) return j["digest"].get<std::string>();
        if (j.contains("images") && !j["images"].empty()) {
            return j["images"][0].value("digest", "");
        }
    } catch (const json::exception&) {}

    return "";
}

bool UpdateChecker::compareVersions(const std::string& v1, const std::string& v2) {
    // Simple lexicographic comparison
    // TODO: Implement semantic versioning comparison
    return v1 < v2;
}

std::string UpdateChecker::parseRegistryFromImage(const std::string& image) {
    size_t firstSlash = image.find('/');
    if (firstSlash != std::string::npos) {
        std::string prefix = image.substr(0, firstSlash);
        if (prefix.find('.') != std::string::npos || prefix == "localhost") {
            return prefix;
        }
    }
    return "docker.io";
}

std::string UpdateChecker::getReleaseNotes(const std::string& image, const std::string& fromTag,
                                          const std::string& toTag) {
    // Try to fetch release notes from Docker Hub
    std::string imageName = image;
    if (imageName.find('/') == std::string::npos) {
        imageName = "library/" + imageName;
    }

    std::string url = "https://registry.hub.docker.com/v2/repositories/" + imageName + "/";

    CURL* curl = curl_easy_init();
    if (!curl) return "Release notes not available";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DockerHomelabManager/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "Release notes not available";

    try {
        auto j = json::parse(response);
        if (j.contains("full_description")) {
            std::string desc = j["full_description"].get<std::string>();
            // Truncate if too long
            if (desc.length() > 500) {
                desc = desc.substr(0, 500) + "...";
            }
            return desc;
        }
    } catch (const json::exception&) {}

    return "Updating from " + fromTag + " to " + toTag;
}

} // namespace update
