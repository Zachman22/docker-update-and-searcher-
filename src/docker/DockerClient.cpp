#include "docker/DockerClient.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace docker {

// Callback for libcurl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Pimpl implementation with CURL handle
class DockerClient::Impl {
public:
    CURL* curl = nullptr;
    std::string dockerHost;

    Impl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
    }

    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    std::string makeHttpRequest(const std::string& endpoint, const std::string& method = "GET",
                               const std::string& data = "") {
        if (!curl) return "";

        std::string response;
        std::string url;

        // Handle Unix socket vs HTTP
        #ifdef _WIN32
        // Windows uses named pipe: //./pipe/docker_engine
        url = "http://localhost/v1.41" + endpoint;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        #else
        // Linux/macOS use Unix socket
        url = "http://localhost/v1.41" + endpoint;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
        #endif

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        // Set HTTP method
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!data.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            }
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        // Perform request
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            LOG_ERROR(std::string("CURL error: ") + curl_easy_strerror(res));
            return "";
        }

        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code >= 400) {
            LOG_ERROR("HTTP error: " + std::to_string(response_code));
            return "";
        }

        return response;
    }
};

DockerClient::DockerClient()
    : pImpl_(std::make_unique<Impl>())
    , connected_(false)
{
    LOG_INFO("DockerClient initialized");
}

DockerClient::~DockerClient() = default;

bool DockerClient::connect(const std::string& dockerHost) {
    #ifdef _WIN32
    dockerHost_ = dockerHost.empty() ? "npipe:////./pipe/docker_engine" : dockerHost;
    #else
    dockerHost_ = dockerHost.empty() ? "unix:///var/run/docker.sock" : dockerHost;
    #endif

    pImpl_->dockerHost = dockerHost_;
    LOG_INFO("Connecting to Docker: " + dockerHost_);

    // Test connection by pinging Docker
    connected_ = ping();

    if (connected_) {
        LOG_INFO("Successfully connected to Docker daemon");
    } else {
        LOG_ERROR("Failed to connect to Docker daemon");
    }

    return connected_;
}

bool DockerClient::isConnected() const {
    return connected_;
}

std::string DockerClient::getDockerVersion() {
    std::string response = pImpl_->makeHttpRequest("/version", "GET");
    if (response.empty()) return "unknown";

    try {
        auto j = json::parse(response);
        return j.value("Version", "unknown");
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("JSON parse error: ") + e.what());
        return "unknown";
    }
}

std::vector<Container> DockerClient::listContainers(bool includeAll) {
    std::vector<Container> containers;

    std::string endpoint = includeAll ? "/containers/json?all=true" : "/containers/json";
    std::string response = pImpl_->makeHttpRequest(endpoint, "GET");

    if (response.empty()) {
        LOG_WARNING("No response from Docker API");
        return containers;
    }

    try {
        auto j = json::parse(response);

        for (const auto& containerJson : j) {
            Container container = parseContainerJson(containerJson.dump());
            containers.push_back(container);
        }

        LOG_INFO("Listed " + std::to_string(containers.size()) + " containers");
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("JSON parse error: ") + e.what());
    }

    return containers;
}

std::optional<Container> DockerClient::getContainer(const std::string& id) {
    std::string response = pImpl_->makeHttpRequest("/containers/" + id + "/json", "GET");

    if (response.empty()) {
        return std::nullopt;
    }

    try {
        Container container = parseContainerJson(response);
        return container;
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("JSON parse error: ") + e.what());
        return std::nullopt;
    }
}

bool DockerClient::startContainer(const std::string& id) {
    LOG_INFO("Starting container: " + id);
    std::string response = pImpl_->makeHttpRequest("/containers/" + id + "/start", "POST");
    return !response.empty() || response == "";  // Docker returns empty on success
}

bool DockerClient::stopContainer(const std::string& id, int timeout) {
    LOG_INFO("Stopping container: " + id);
    std::string endpoint = "/containers/" + id + "/stop?t=" + std::to_string(timeout);
    std::string response = pImpl_->makeHttpRequest(endpoint, "POST");
    return true;  // Docker returns 204 or 304 on success
}

bool DockerClient::restartContainer(const std::string& id) {
    LOG_INFO("Restarting container: " + id);
    std::string response = pImpl_->makeHttpRequest("/containers/" + id + "/restart", "POST");
    return true;
}

bool DockerClient::removeContainer(const std::string& id, bool force) {
    LOG_INFO("Removing container: " + id);
    std::string endpoint = "/containers/" + id + (force ? "?force=true" : "");
    std::string response = pImpl_->makeHttpRequest(endpoint, "DELETE");
    return true;
}

std::string DockerClient::getContainerLogs(const std::string& id, int lines) {
    std::string endpoint = "/containers/" + id + "/logs?stdout=true&stderr=true&tail=" + std::to_string(lines);
    return pImpl_->makeHttpRequest(endpoint, "GET");
}

bool DockerClient::pullImage(const std::string& image, const std::string& tag) {
    LOG_INFO("Pulling image: " + image + ":" + tag);
    std::string endpoint = "/images/create?fromImage=" + image + "&tag=" + tag;
    std::string response = pImpl_->makeHttpRequest(endpoint, "POST");
    return !response.empty();
}

bool DockerClient::removeImage(const std::string& image) {
    LOG_INFO("Removing image: " + image);
    std::string response = pImpl_->makeHttpRequest("/images/" + image, "DELETE");
    return !response.empty();
}

std::vector<std::string> DockerClient::listImages() {
    std::vector<std::string> images;
    std::string response = pImpl_->makeHttpRequest("/images/json", "GET");

    if (response.empty()) return images;

    try {
        auto j = json::parse(response);
        for (const auto& img : j) {
            if (img.contains("RepoTags") && !img["RepoTags"].empty()) {
                for (const auto& tag : img["RepoTags"]) {
                    images.push_back(tag.get<std::string>());
                }
            }
        }
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("JSON parse error: ") + e.what());
    }

    return images;
}

bool DockerClient::imageExists(const std::string& image, const std::string& tag) {
    std::string fullImage = image + ":" + tag;
    std::string response = pImpl_->makeHttpRequest("/images/" + fullImage + "/json", "GET");
    return !response.empty();
}

std::vector<std::string> DockerClient::listNetworks() {
    std::vector<std::string> networks;
    std::string response = pImpl_->makeHttpRequest("/networks", "GET");

    if (response.empty()) return networks;

    try {
        auto j = json::parse(response);
        for (const auto& net : j) {
            if (net.contains("Name")) {
                networks.push_back(net["Name"].get<std::string>());
            }
        }
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("JSON parse error: ") + e.what());
    }

    return networks;
}

bool DockerClient::createNetwork(const std::string& name) {
    json data;
    data["Name"] = name;
    std::string response = pImpl_->makeHttpRequest("/networks/create", "POST", data.dump());
    return !response.empty();
}

bool DockerClient::removeNetwork(const std::string& name) {
    std::string response = pImpl_->makeHttpRequest("/networks/" + name, "DELETE");
    return true;
}

std::vector<std::string> DockerClient::listVolumes() {
    std::vector<std::string> volumes;
    std::string response = pImpl_->makeHttpRequest("/volumes", "GET");

    if (response.empty()) return volumes;

    try {
        auto j = json::parse(response);
        if (j.contains("Volumes")) {
            for (const auto& vol : j["Volumes"]) {
                if (vol.contains("Name")) {
                    volumes.push_back(vol["Name"].get<std::string>());
                }
            }
        }
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("JSON parse error: ") + e.what());
    }

    return volumes;
}

bool DockerClient::createVolume(const std::string& name) {
    json data;
    data["Name"] = name;
    std::string response = pImpl_->makeHttpRequest("/volumes/create", "POST", data.dump());
    return !response.empty();
}

bool DockerClient::removeVolume(const std::string& name) {
    std::string response = pImpl_->makeHttpRequest("/volumes/" + name, "DELETE");
    return true;
}

bool DockerClient::login(const std::string& registry, const std::string& username,
                        const std::string& password) {
    LOG_INFO("Logging in to registry: " + registry);
    json auth;
    auth["username"] = username;
    auth["password"] = password;
    auth["serveraddress"] = registry;

    std::string response = pImpl_->makeHttpRequest("/auth", "POST", auth.dump());
    return !response.empty();
}

bool DockerClient::logout(const std::string& registry) {
    // Docker CLI handles logout, not API
    return true;
}

bool DockerClient::ping() {
    std::string response = pImpl_->makeHttpRequest("/_ping", "GET");
    return response == "OK";
}

std::string DockerClient::getSystemInfo() {
    return pImpl_->makeHttpRequest("/info", "GET");
}

std::string DockerClient::makeRequest(const std::string& endpoint, const std::string& method,
                                     const std::string& data) {
    return pImpl_->makeHttpRequest(endpoint, method, data);
}

Container DockerClient::parseContainerJson(const std::string& jsonStr) {
    Container container;

    try {
        auto j = json::parse(jsonStr);

        // Basic info
        container.setId(j.value("Id", ""));

        // Parse names (remove leading /)
        if (j.contains("Names") && !j["Names"].empty()) {
            std::string name = j["Names"][0].get<std::string>();
            if (!name.empty() && name[0] == '/') {
                name = name.substr(1);
            }
            container.setName(name);
        } else if (j.contains("Name")) {
            std::string name = j["Name"].get<std::string>();
            if (!name.empty() && name[0] == '/') {
                name = name.substr(1);
            }
            container.setName(name);
        }

        // Parse image
        if (j.contains("Image")) {
            std::string image = j["Image"].get<std::string>();
            container.setImage(image);

            // Extract tag
            size_t colonPos = image.find_last_of(':');
            if (colonPos != std::string::npos) {
                container.setImageTag(image.substr(colonPos + 1));
            } else {
                container.setImageTag("latest");
            }
        }

        // Parse state
        std::string state = j.value("State", "unknown");
        if (state == "running") {
            container.setState(ContainerState::Running);
        } else if (state == "exited") {
            container.setState(ContainerState::Exited);
        } else if (state == "paused") {
            container.setState(ContainerState::Paused);
        } else if (state == "restarting") {
            container.setState(ContainerState::Restarting);
        } else if (state == "dead") {
            container.setState(ContainerState::Dead);
        } else if (state == "created") {
            container.setState(ContainerState::Created);
        } else {
            container.setState(ContainerState::Unknown);
        }

        // Parse status
        container.setStatus(j.value("Status", ""));

        // Parse ports
        if (j.contains("Ports")) {
            std::vector<PortMapping> ports;
            for (const auto& portJson : j["Ports"]) {
                PortMapping pm;
                pm.containerPort = portJson.value("PrivatePort", 0);
                pm.hostPort = portJson.value("PublicPort", 0);
                pm.protocol = portJson.value("Type", "tcp");
                ports.push_back(pm);
            }
            container.setPorts(ports);
        }

        // Parse networks
        if (j.contains("NetworkSettings") && j["NetworkSettings"].contains("Networks")) {
            std::vector<NetworkConnection> networks;
            for (auto& [name, netInfo] : j["NetworkSettings"]["Networks"].items()) {
                NetworkConnection nc;
                nc.networkName = name;
                nc.ipAddress = netInfo.value("IPAddress", "");
                nc.gateway = netInfo.value("Gateway", "");
                nc.networkId = netInfo.value("NetworkID", "");
                networks.push_back(nc);
            }
            container.setNetworks(networks);
        }

    } catch (const json::exception& e) {
        LOG_ERROR(std::string("Error parsing container JSON: ") + e.what());
    }

    return container;
}

std::string Container::getStateString() const {
    switch (state_) {
        case ContainerState::Running: return "Running";
        case ContainerState::Stopped: return "Stopped";
        case ContainerState::Paused: return "Paused";
        case ContainerState::Restarting: return "Restarting";
        case ContainerState::Dead: return "Dead";
        case ContainerState::Created: return "Created";
        case ContainerState::Exited: return "Exited";
        default: return "Unknown";
    }
}

Container::Container(const std::string& id, const std::string& name)
    : id_(id), name_(name) {}

} // namespace docker
