#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "Container.h"

namespace docker {

// Image structure
struct Image {
    std::string id;
    std::vector<std::string> repoTags;
    std::vector<std::string> repoDigests;
    int64_t size = 0;
    std::string created;
};

// Network structure
struct Network {
    std::string id;
    std::string name;
    std::string driver;
    std::string scope;
};

// Volume structure
struct Volume {
    std::string name;
    std::string driver;
    std::string mountpoint;
};

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
    std::optional<std::string> createContainer(const nlohmann::json& config, const std::string& name = "");
    bool startContainer(const std::string& id);
    bool stopContainer(const std::string& id, int timeout = 10);
    bool restartContainer(const std::string& id);
    bool removeContainer(const std::string& id, bool force = false, bool removeVolumes = false);
    std::string getContainerLogs(const std::string& id, int lines = 100);
    std::string execInContainer(const std::string& id, const std::vector<std::string>& command);

    // Image operations
    bool pullImage(const std::string& image, const std::string& tag = "latest");
    bool removeImage(const std::string& image);
    std::vector<Image> listImages();
    std::vector<std::string> listImageNames();  // Legacy method
    bool imageExists(const std::string& image, const std::string& tag);

    // Network operations
    std::vector<Network> listNetworks();
    std::vector<std::string> listNetworkNames();  // Legacy method
    bool createNetwork(const std::string& name, const std::string& driver = "bridge");
    bool removeNetwork(const std::string& name);
    bool connectContainerToNetwork(const std::string& containerId, const std::string& networkName);
    bool disconnectContainerFromNetwork(const std::string& containerId, const std::string& networkName);

    // Volume operations
    std::vector<Volume> listVolumes();
    std::vector<std::string> listVolumeNames();  // Legacy method
    bool createVolume(const std::string& name, const std::string& driver = "local");
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
