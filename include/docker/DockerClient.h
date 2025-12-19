#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "Container.h"

namespace docker {

class DockerClient {
public:
    DockerClient();
    ~DockerClient();

    // Connection management
    bool connect(const std::string& dockerHost = "");
    bool isConnected() const;
    std::string getDockerVersion();

    // Container operations
    std::vector<Container> listContainers(bool includeAll = true);
    std::optional<Container> getContainer(const std::string& id);
    bool startContainer(const std::string& id);
    bool stopContainer(const std::string& id, int timeout = 10);
    bool restartContainer(const std::string& id);
    bool removeContainer(const std::string& id, bool force = false);
    std::string getContainerLogs(const std::string& id, int lines = 100);

    // Image operations
    bool pullImage(const std::string& image, const std::string& tag = "latest");
    bool removeImage(const std::string& image);
    std::vector<std::string> listImages();
    bool imageExists(const std::string& image, const std::string& tag);

    // Network operations
    std::vector<std::string> listNetworks();
    bool createNetwork(const std::string& name);
    bool removeNetwork(const std::string& name);

    // Volume operations
    std::vector<std::string> listVolumes();
    bool createVolume(const std::string& name);
    bool removeVolume(const std::string& name);

    // Registry operations
    bool login(const std::string& registry, const std::string& username, const std::string& password);
    bool logout(const std::string& registry);

    // System operations
    bool ping();
    std::string getSystemInfo();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    std::string dockerHost_;
    bool connected_;

    // Helper methods
    std::string makeRequest(const std::string& endpoint, const std::string& method = "GET",
                           const std::string& data = "");
    Container parseContainerJson(const std::string& json);
};

} // namespace docker
