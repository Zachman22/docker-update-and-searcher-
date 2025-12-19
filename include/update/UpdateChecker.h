#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include "../docker/Container.h"

namespace update {

struct ImageVersion {
    std::string tag;
    std::string digest;
    std::chrono::system_clock::time_point publishedDate;
    size_t sizeBytes;
    std::vector<std::string> layers;
};

struct UpdateInfo {
    std::string containerId;
    std::string containerName;
    std::string currentImage;
    std::string currentTag;
    std::string latestTag;
    std::string currentDigest;
    std::string latestDigest;
    bool updateAvailable;
    std::vector<std::string> dependencies;
    std::string releaseNotes;
};

enum class UpdateStrategy {
    Conservative,  // Only update to stable releases
    Moderate,      // Update to latest stable (default)
    Aggressive,    // Update to latest including pre-releases
    Custom         // User-defined rules
};

class UpdateChecker {
public:
    UpdateChecker();
    ~UpdateChecker();

    // Update checking
    std::vector<UpdateInfo> checkForUpdates(const std::vector<docker::Container>& containers);
    std::optional<UpdateInfo> checkContainerUpdate(const docker::Container& container);
    bool isUpdateAvailable(const std::string& image, const std::string& currentTag);

    // Registry operations
    std::vector<ImageVersion> getAvailableVersions(const std::string& image);
    std::optional<ImageVersion> getLatestVersion(const std::string& image, UpdateStrategy strategy);
    bool authenticateRegistry(const std::string& registry, const std::string& username,
                             const std::string& password);

    // Update operations
    bool performUpdate(const UpdateInfo& updateInfo, bool backupFirst = true);
    bool performBatchUpdate(const std::vector<UpdateInfo>& updates);
    bool rollbackUpdate(const std::string& containerId);

    // Backup and rollback
    bool createBackup(const std::string& containerId);
    bool restoreBackup(const std::string& containerId);
    std::vector<std::string> listBackups(const std::string& containerId);

    // Configuration
    void setUpdateStrategy(UpdateStrategy strategy);
    UpdateStrategy getUpdateStrategy() const;
    void addExcludedContainer(const std::string& containerName);
    void removeExcludedContainer(const std::string& containerName);
    bool isContainerExcluded(const std::string& containerName) const;

    // Scheduling
    void setAutoUpdateEnabled(bool enabled);
    bool isAutoUpdateEnabled() const;
    void setUpdateSchedule(const std::string& cronExpression);

private:
    UpdateStrategy strategy_;
    std::vector<std::string> excludedContainers_;
    bool autoUpdateEnabled_;
    std::string updateSchedule_;

    // Helper methods
    std::string fetchLatestTag(const std::string& image);
    std::string fetchDigest(const std::string& image, const std::string& tag);
    bool compareVersions(const std::string& v1, const std::string& v2);
    std::string parseRegistryFromImage(const std::string& image);
    std::string getReleaseNotes(const std::string& image, const std::string& fromTag,
                               const std::string& toTag);
};

} // namespace update
