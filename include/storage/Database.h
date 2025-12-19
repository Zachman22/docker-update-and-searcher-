#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>
#include "../docker/Container.h"
#include "../diagnostics/ErrorDiagnostics.h"

namespace storage {

struct ContainerHistory {
    std::string containerId;
    std::string action; // "created", "started", "stopped", "updated", etc.
    std::string timestamp;
    std::string details;
    std::string performedBy; // "user" or "auto"
};

struct UpdateHistory {
    std::string containerId;
    std::string fromVersion;
    std::string toVersion;
    std::string timestamp;
    bool success;
    std::string errorMessage;
    bool rolledBack;
};

struct AppSettings {
    bool autoUpdate;
    std::string updateSchedule;
    std::string updateStrategy;
    bool notificationsEnabled;
    std::string dockerHost;
    int portScanRange;
    bool enableHealthMonitoring;
    int healthCheckInterval;
};

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    // Database management
    bool initialize();
    bool migrate();
    bool close();
    bool isOpen() const;

    // Container tracking
    bool saveContainer(const docker::Container& container);
    bool updateContainer(const docker::Container& container);
    std::optional<docker::Container> getContainer(const std::string& id);
    std::vector<docker::Container> getAllContainers();
    bool deleteContainer(const std::string& id);

    // History tracking
    bool recordContainerAction(const ContainerHistory& history);
    std::vector<ContainerHistory> getContainerHistory(const std::string& containerId, int limit = 100);
    std::vector<ContainerHistory> getRecentHistory(int limit = 50);

    // Update history
    bool recordUpdate(const UpdateHistory& update);
    std::vector<UpdateHistory> getUpdateHistory(const std::string& containerId);
    std::optional<UpdateHistory> getLastUpdate(const std::string& containerId);
    std::vector<UpdateHistory> getFailedUpdates();

    // Issue tracking
    bool saveIssue(const diagnostics::DiagnosticIssue& issue);
    bool updateIssue(const diagnostics::DiagnosticIssue& issue);
    std::vector<diagnostics::DiagnosticIssue> getActiveIssues();
    std::vector<diagnostics::DiagnosticIssue> getResolvedIssues(int limit = 100);
    bool markIssueResolved(const std::string& issueId);

    // Settings management
    bool saveSettings(const AppSettings& settings);
    std::optional<AppSettings> getSettings();
    bool updateSetting(const std::string& key, const std::string& value);

    // Statistics
    int getTotalContainers();
    int getRunningContainers();
    int getPendingUpdates();
    int getActiveIssuesCount();
    std::map<std::string, int> getIssuesByCategoryCount();

    // Cleanup
    bool cleanupOldHistory(int daysToKeep = 30);
    bool cleanupResolvedIssues(int daysToKeep = 7);
    bool vacuum();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    std::string dbPath_;
    bool isOpen_;

    // Schema management
    bool createTables();
    bool upgradeSchema(int fromVersion, int toVersion);
    int getCurrentSchemaVersion();
    bool setSchemaVersion(int version);

    // Helper methods
    std::string serializeContainer(const docker::Container& container);
    docker::Container deserializeContainer(const std::string& data);
};

} // namespace storage
