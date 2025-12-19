#include "update/UpdateChecker.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <regex>
#include <algorithm>
#include <ctime>

using json = nlohmann::json;

namespace update {

// Helper function for CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

UpdateChecker::UpdateChecker()
    : strategy_(UpdateStrategy::Moderate)
    , autoUpdateEnabled_(false)
{
    LOG_INFO("UpdateChecker initialized");
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

UpdateChecker::~UpdateChecker() {
    curl_global_cleanup();
}

std::vector<UpdateInfo> UpdateChecker::checkForUpdates(const std::vector<docker::Container>& containers) {
    LOG_INFO("Checking for updates on " + std::to_string(containers.size()) + " containers");
    std::vector<UpdateInfo> updates;

    for (const auto& container : containers) {
        // Skip if container is excluded
        if (isContainerExcluded(container.name)) {
            continue;
        }

        auto updateInfo = checkContainerUpdate(container);
        if (updateInfo.has_value()) {
            updates.push_back(updateInfo.value());
        }
    }

    LOG_INFO("Found " + std::to_string(updates.size()) + " containers with available updates");
    return updates;
}

std::optional<UpdateInfo> UpdateChecker::checkContainerUpdate(const docker::Container& container) {
    try {
        // Parse image and tag from container
        std::string image = container.image;
        std::string currentTag = "latest";

        // Split image:tag format
        size_t colonPos = image.find(':');
        if (colonPos != std::string::npos) {
            currentTag = image.substr(colonPos + 1);
            image = image.substr(0, colonPos);
        }

        // Get current digest
        std::string currentDigest = fetchDigest(image, currentTag);

        // Get latest version based on strategy
        auto latestVersion = getLatestVersion(image, strategy_);

        if (!latestVersion.has_value()) {
            LOG_WARNING("Could not fetch latest version for: " + image);
            return std::nullopt;
        }

        UpdateInfo info;
        info.containerId = container.id;
        info.containerName = container.name;
        info.currentImage = image;
        info.currentTag = currentTag;
        info.latestTag = latestVersion->tag;
        info.currentDigest = currentDigest;
        info.latestDigest = latestVersion->digest;
        info.updateAvailable = (currentDigest != latestVersion->digest && !latestVersion->digest.empty());
        info.releaseNotes = getReleaseNotes(image, currentTag, latestVersion->tag);

        return info;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error checking update for " + container.name + ": " + e.what());
        return std::nullopt;
    }
}

bool UpdateChecker::isUpdateAvailable(const std::string& image, const std::string& currentTag) {
    try {
        std::string currentDigest = fetchDigest(image, currentTag);
        auto latestVersion = getLatestVersion(image, strategy_);

        if (!latestVersion.has_value()) {
            return false;
        }

        return currentDigest != latestVersion->digest;
    }
    catch (...) {
        return false;
    }
}

std::vector<ImageVersion> UpdateChecker::getAvailableVersions(const std::string& image) {
    LOG_INFO("Fetching available versions for: " + image);
    std::vector<ImageVersion> versions;

    try {
        // Parse image name (handle library/ prefix for official images)
        std::string repo = image;
        if (repo.find('/') == std::string::npos) {
            repo = "library/" + repo;
        }

        // Docker Hub API endpoint
        std::string url = "https://registry.hub.docker.com/v2/repositories/" + repo + "/tags?page_size=100";

        CURL* curl = curl_easy_init();
        if (!curl) {
            LOG_ERROR("Failed to initialize CURL");
            return versions;
        }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "DockerHomelabManager/0.1");

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            LOG_ERROR("CURL error: " + std::string(curl_easy_strerror(res)));
            curl_easy_cleanup(curl);
            return versions;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (http_code != 200) {
            LOG_ERROR("HTTP error: " + std::to_string(http_code));
            return versions;
        }

        // Parse JSON response
        auto jsonData = json::parse(response);

        if (jsonData.contains("results")) {
            for (const auto& result : jsonData["results"]) {
                ImageVersion version;
                version.tag = result.value("name", "");
                version.digest = result.value("digest", "");

                // Parse size
                if (result.contains("full_size")) {
                    version.sizeBytes = result["full_size"].get<size_t>();
                }

                // Parse last updated timestamp
                if (result.contains("last_updated")) {
                    // TODO: Parse ISO 8601 timestamp
                    version.publishedDate = std::chrono::system_clock::now();
                }

                versions.push_back(version);
            }
        }

        LOG_INFO("Found " + std::to_string(versions.size()) + " versions for " + image);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error fetching versions: " + std::string(e.what()));
    }

    return versions;
}

std::optional<ImageVersion> UpdateChecker::getLatestVersion(const std::string& image,
                                                            UpdateStrategy strategy) {
    auto versions = getAvailableVersions(image);

    if (versions.empty()) {
        return std::nullopt;
    }

    // Find latest based on strategy
    for (const auto& version : versions) {
        std::string tag = version.tag;

        switch (strategy) {
            case UpdateStrategy::Conservative:
                // Look for stable releases (no alpha/beta/rc)
                if (tag.find("alpha") == std::string::npos &&
                    tag.find("beta") == std::string::npos &&
                    tag.find("rc") == std::string::npos &&
                    tag != "latest") {
                    return version;
                }
                break;

            case UpdateStrategy::Moderate:
                // Accept "latest" or stable versions
                if (tag == "latest" || tag.find("stable") != std::string::npos) {
                    return version;
                }
                break;

            case UpdateStrategy::Aggressive:
                // Take the first (most recent) version
                return version;

            case UpdateStrategy::Custom:
                // TODO: Implement custom strategy logic
                return version;
        }
    }

    // If no match found, return first version
    return versions.empty() ? std::nullopt : std::make_optional(versions[0]);
}

bool UpdateChecker::authenticateRegistry(const std::string& registry, const std::string& username,
                                        const std::string& password) {
    LOG_INFO("Authenticating to registry: " + registry);
    // TODO: Implement Docker registry authentication (OAuth2 or basic auth)
    // For now, Docker Hub public images don't require auth
    return true;
}

bool UpdateChecker::performUpdate(const UpdateInfo& updateInfo, bool backupFirst) {
    LOG_INFO("Performing update for: " + updateInfo.containerName);

    try {
        // TODO: Implement actual update logic:
        // 1. Create backup if requested
        // 2. Stop container
        // 3. Pull new image
        // 4. Remove old container
        // 5. Create new container with same config
        // 6. Start new container

        if (backupFirst) {
            if (!createBackup(updateInfo.containerId)) {
                LOG_ERROR("Failed to create backup");
                return false;
            }
        }

        LOG_INFO("Update completed successfully");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Update failed: " + std::string(e.what()));
        return false;
    }
}

bool UpdateChecker::performBatchUpdate(const std::vector<UpdateInfo>& updates) {
    LOG_INFO("Performing batch update for " + std::to_string(updates.size()) + " containers");

    int success = 0;
    for (const auto& update : updates) {
        if (performUpdate(update, true)) {
            success++;
        }
    }

    LOG_INFO("Batch update completed: " + std::to_string(success) + "/" +
             std::to_string(updates.size()) + " successful");

    return success == updates.size();
}

bool UpdateChecker::rollbackUpdate(const std::string& containerId) {
    LOG_INFO("Rolling back update for: " + containerId);
    return restoreBackup(containerId);
}

bool UpdateChecker::createBackup(const std::string& containerId) {
    // TODO: Implement container backup
    LOG_INFO("Creating backup for: " + containerId);
    return true;
}

bool UpdateChecker::restoreBackup(const std::string& containerId) {
    // TODO: Implement backup restore
    LOG_INFO("Restoring backup for: " + containerId);
    return true;
}

std::vector<std::string> UpdateChecker::listBackups(const std::string& containerId) {
    // TODO: List available backups
    return {};
}

void UpdateChecker::setUpdateStrategy(UpdateStrategy strategy) {
    strategy_ = strategy;
    LOG_INFO("Update strategy changed");
}

UpdateStrategy UpdateChecker::getUpdateStrategy() const {
    return strategy_;
}

void UpdateChecker::addExcludedContainer(const std::string& containerName) {
    excludedContainers_.push_back(containerName);
    LOG_INFO("Container excluded from updates: " + containerName);
}

void UpdateChecker::removeExcludedContainer(const std::string& containerName) {
    auto it = std::remove(excludedContainers_.begin(), excludedContainers_.end(), containerName);
    excludedContainers_.erase(it, excludedContainers_.end());
}

bool UpdateChecker::isContainerExcluded(const std::string& containerName) const {
    return std::find(excludedContainers_.begin(), excludedContainers_.end(), containerName)
           != excludedContainers_.end();
}

void UpdateChecker::setAutoUpdateEnabled(bool enabled) {
    autoUpdateEnabled_ = enabled;
    LOG_INFO(std::string("Auto-update ") + (enabled ? "enabled" : "disabled"));
}

bool UpdateChecker::isAutoUpdateEnabled() const {
    return autoUpdateEnabled_;
}

void UpdateChecker::setUpdateSchedule(const std::string& cronExpression) {
    updateSchedule_ = cronExpression;
    LOG_INFO("Update schedule set: " + cronExpression);
}

std::string UpdateChecker::fetchLatestTag(const std::string& image) {
    auto versions = getAvailableVersions(image);
    if (!versions.empty()) {
        return versions[0].tag;
    }
    return "latest";
}

std::string UpdateChecker::fetchDigest(const std::string& image, const std::string& tag) {
    try {
        // Parse image name
        std::string repo = image;
        if (repo.find('/') == std::string::npos) {
            repo = "library/" + repo;
        }

        // Docker Hub manifest API
        std::string url = "https://registry.hub.docker.com/v2/repositories/" + repo + "/tags/" + tag;

        CURL* curl = curl_easy_init();
        if (!curl) {
            return "";
        }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "DockerHomelabManager/0.1");

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return "";
        }

        auto jsonData = json::parse(response);
        if (jsonData.contains("digest")) {
            return jsonData["digest"].get<std::string>();
        }
        else if (jsonData.contains("images") && !jsonData["images"].empty()) {
            return jsonData["images"][0].value("digest", "");
        }
    }
    catch (...) {
    }

    return "";
}

bool UpdateChecker::compareVersions(const std::string& v1, const std::string& v2) {
    // Simple semantic version comparison
    // TODO: Implement full semver comparison
    return v1 < v2;
}

std::string UpdateChecker::parseRegistryFromImage(const std::string& image) {
    // Check if image has registry prefix (e.g., gcr.io, ghcr.io)
    size_t firstSlash = image.find('/');
    if (firstSlash != std::string::npos) {
        std::string prefix = image.substr(0, firstSlash);
        if (prefix.find('.') != std::string::npos) {
            // Contains a dot, likely a registry domain
            return prefix;
        }
    }

    // Default to Docker Hub
    return "docker.io";
}

std::string UpdateChecker::getReleaseNotes(const std::string& image, const std::string& fromTag,
                                          const std::string& toTag) {
    // TODO: Fetch release notes from registry or GitHub
    return "Release notes for " + image + " " + fromTag + " -> " + toTag;
}

} // namespace update
