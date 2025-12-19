#include "update/UpdateChecker.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

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

    for (const auto& v : versions) {
        if (v.tag == "latest") return v;
    }

    return versions.empty() ? std::nullopt : std::make_optional(versions[0]);
}

bool UpdateChecker::authenticateRegistry(const std::string& registry, const std::string& username,
                                        const std::string& password) {
    LOG_INFO("Authenticating to registry: " + registry);
    return false;
}

bool UpdateChecker::performUpdate(const UpdateInfo& updateInfo, bool backupFirst) {
    LOG_INFO("Performing update for: " + updateInfo.containerName);
    return false;
}

bool UpdateChecker::performBatchUpdate(const std::vector<UpdateInfo>& updates) {
    LOG_INFO("Performing batch update");
    return false;
}

bool UpdateChecker::rollbackUpdate(const std::string& containerId) { return false; }
bool UpdateChecker::createBackup(const std::string& containerId) { return false; }
bool UpdateChecker::restoreBackup(const std::string& containerId) { return false; }
std::vector<std::string> UpdateChecker::listBackups(const std::string& containerId) { return {}; }

void UpdateChecker::setUpdateStrategy(UpdateStrategy strategy) { strategy_ = strategy; }
UpdateStrategy UpdateChecker::getUpdateStrategy() const { return strategy_; }

void UpdateChecker::addExcludedContainer(const std::string& containerName) {
    excludedContainers_.push_back(containerName);
}

void UpdateChecker::removeExcludedContainer(const std::string& containerName) {
    auto it = std::find(excludedContainers_.begin(), excludedContainers_.end(), containerName);
    if (it != excludedContainers_.end()) excludedContainers_.erase(it);
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
    return "Release notes not available";
}

} // namespace update
