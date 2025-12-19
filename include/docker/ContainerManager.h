#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "Container.h"
#include "DockerClient.h"

namespace docker {

struct DependencyInfo {
    std::string containerId;
    std::string containerName;
    std::vector<std::string> requiredBy;
    std::vector<std::string> requires;
    int startOrder; // Lower numbers start first
};

class ContainerManager {
public:
    explicit ContainerManager(std::shared_ptr<DockerClient> client);
    ~ContainerManager();

    // Container discovery and management
    bool refreshContainers();
    std::vector<Container> getAllContainers() const;
    std::vector<Container> getRunningContainers() const;
    std::vector<Container> getStoppedContainers() const;
    std::optional<Container> findContainer(const std::string& nameOrId);

    // Dependency management
    std::vector<DependencyInfo> analyzeDependencies();
    std::vector<std::string> getStartupOrder(const std::vector<std::string>& containerIds);
    bool hasDependencies(const std::string& containerId);
    std::vector<std::string> getDependentContainers(const std::string& containerId);

    // Batch operations
    bool startContainers(const std::vector<std::string>& ids);
    bool stopContainers(const std::vector<std::string>& ids);
    bool restartContainers(const std::vector<std::string>& ids);

    // Safe operations with dependency awareness
    bool safeStopContainer(const std::string& id, bool stopDependents = false);
    bool safeStartContainer(const std::string& id, bool startDependencies = true);
    bool safeRestartContainer(const std::string& id);

    // Health monitoring
    bool isContainerHealthy(const std::string& id);
    std::string getContainerHealth(const std::string& id);

    // Events and callbacks
    using ContainerEventCallback = std::function<void(const Container&, const std::string& event)>;
    void setEventCallback(ContainerEventCallback callback);

private:
    std::shared_ptr<DockerClient> client_;
    std::vector<Container> containers_;
    ContainerEventCallback eventCallback_;

    // Helper methods
    void detectDependencies();
    bool isNetworkDependency(const Container& c1, const Container& c2);
    bool isVolumeDependency(const Container& c1, const Container& c2);
    bool isLinkDependency(const Container& c1, const Container& c2);
};

} // namespace docker
