#include "docker/DependencyResolver.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <queue>
#include <sstream>

using json = nlohmann::json;

namespace docker {

// Helper for CURL responses
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

DependencyResolver::DependencyResolver() {
    LOG_INFO("DependencyResolver initialized");
}

DependencyResolver::~DependencyResolver() = default;

std::vector<Dependency> DependencyResolver::analyzeDependencies(const std::string& containerId) {
    LOG_INFO("Analyzing dependencies for container: " + containerId);

    std::vector<Dependency> dependencies;

    // Query container details from Docker API
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return dependencies;
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
        LOG_ERROR("Failed to fetch container details");
        return dependencies;
    }

    try {
        auto j = json::parse(response);

        // Check network dependencies
        if (j.contains("NetworkSettings") && j["NetworkSettings"].contains("Networks")) {
            for (auto& [networkName, networkInfo] : j["NetworkSettings"]["Networks"].items()) {
                Dependency dep;
                dep.sourceContainer = containerId;
                dep.targetContainer = "";
                dep.type = DependencyType::Network;
                dep.details = networkName;
                dep.isSatisfied = true; // Network exists if container is using it
                dependencies.push_back(dep);
            }
        }

        // Check volume mounts
        if (j.contains("Mounts")) {
            for (auto& mount : j["Mounts"]) {
                if (mount.contains("Type") && mount["Type"] == "volume") {
                    Dependency dep;
                    dep.sourceContainer = containerId;
                    dep.targetContainer = "";
                    dep.type = DependencyType::VolumeMount;
                    dep.details = mount.value("Name", "");
                    dep.isSatisfied = true; // Volume exists if mounted
                    dependencies.push_back(dep);
                }
            }
        }

        // Check port dependencies
        if (j.contains("HostConfig") && j["HostConfig"].contains("PortBindings")) {
            for (auto& [containerPort, hostBindings] : j["HostConfig"]["PortBindings"].items()) {
                if (!hostBindings.empty() && hostBindings[0].contains("HostPort")) {
                    Dependency dep;
                    dep.sourceContainer = containerId;
                    dep.targetContainer = "";
                    dep.type = DependencyType::PortConflict;
                    dep.details = hostBindings[0]["HostPort"].get<std::string>();
                    dep.isSatisfied = true; // Port is available if container is running
                    dependencies.push_back(dep);
                }
            }
        }

        // Check image dependency
        if (j.contains("Image")) {
            Dependency dep;
            dep.sourceContainer = containerId;
            dep.targetContainer = "";
            dep.type = DependencyType::ImageBase;
            dep.details = j["Image"].get<std::string>();
            dep.isSatisfied = true; // Image exists if container exists
            dependencies.push_back(dep);
        }

    } catch (const json::exception& e) {
        LOG_ERROR("JSON parsing error: " + std::string(e.what()));
    }

    return dependencies;
}

std::vector<Dependency> DependencyResolver::findAllDependencies() {
    LOG_INFO("Finding all container dependencies");

    std::vector<Dependency> allDependencies;

    // Get all containers
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return allDependencies;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/containers/json?all=true";
#else
    std::string url = "http://localhost/v1.41/containers/json?all=true";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to fetch containers list");
        return allDependencies;
    }

    try {
        auto j = json::parse(response);

        for (auto& container : j) {
            std::string containerId = container.value("Id", "");
            auto deps = analyzeDependencies(containerId);
            allDependencies.insert(allDependencies.end(), deps.begin(), deps.end());
        }

    } catch (const json::exception& e) {
        LOG_ERROR("JSON parsing error: " + std::string(e.what()));
    }

    return allDependencies;
}

DependencyGraph DependencyResolver::buildDependencyGraph() {
    LOG_INFO("Building dependency graph");

    DependencyGraph graph;
    auto allDeps = findAllDependencies();

    for (const auto& dep : allDeps) {
        if (!dep.targetContainer.empty()) {
            graph.dependencies[dep.sourceContainer].push_back(dep.targetContainer);
            graph.dependents[dep.targetContainer].push_back(dep.sourceContainer);
        }
    }

    return graph;
}

bool DependencyResolver::areDependenciesSatisfied(const std::string& containerId) {
    LOG_INFO("Checking if dependencies are satisfied for: " + containerId);

    auto deps = analyzeDependencies(containerId);

    for (const auto& dep : deps) {
        if (!dep.isSatisfied) {
            LOG_WARNING("Unsatisfied dependency: " + dep.details);
            return false;
        }
    }

    return true;
}

std::vector<std::string> DependencyResolver::findUnsatisfiedDependencies(const std::string& containerId) {
    LOG_INFO("Finding unsatisfied dependencies for: " + containerId);

    std::vector<std::string> unsatisfied;
    auto deps = analyzeDependencies(containerId);

    for (const auto& dep : deps) {
        if (!dep.isSatisfied) {
            std::string msg = "Type: ";
            switch (dep.type) {
                case DependencyType::Network:
                    msg += "Network - " + dep.details;
                    break;
                case DependencyType::VolumeMount:
                    msg += "Volume - " + dep.details;
                    break;
                case DependencyType::PortConflict:
                    msg += "Port - " + dep.details;
                    break;
                case DependencyType::ImageBase:
                    msg += "Image - " + dep.details;
                    break;
                default:
                    msg += "Unknown";
            }
            msg += " - " + dep.unsatisfiedReason;
            unsatisfied.push_back(msg);
        }
    }

    return unsatisfied;
}

std::vector<std::string> DependencyResolver::findMissingImages() {
    LOG_INFO("Finding missing images");

    std::vector<std::string> missingImages;

    // Get all containers
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return missingImages;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/containers/json?all=true";
#else
    std::string url = "http://localhost/v1.41/containers/json?all=true";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to fetch containers");
        return missingImages;
    }

    try {
        auto containers = json::parse(response);

        // Get all images
        curl = curl_easy_init();
        if (!curl) {
            return missingImages;
        }

        std::string imagesResponse;

#ifdef _WIN32
        url = "http://localhost/v1.41/images/json";
#else
        url = "http://localhost/v1.41/images/json";
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &imagesResponse);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return missingImages;
        }

        auto images = json::parse(imagesResponse);
        std::set<std::string> availableImages;

        for (const auto& img : images) {
            if (img.contains("RepoTags")) {
                for (const auto& tag : img["RepoTags"]) {
                    availableImages.insert(tag.get<std::string>());
                }
            }
        }

        // Check each container's image
        for (const auto& container : containers) {
            std::string imageName = container.value("Image", "");
            if (!imageName.empty() && availableImages.find(imageName) == availableImages.end()) {
                missingImages.push_back(imageName);
            }
        }

    } catch (const json::exception& e) {
        LOG_ERROR("JSON parsing error: " + std::string(e.what()));
    }

    return missingImages;
}

std::vector<std::string> DependencyResolver::findMissingNetworks() {
    LOG_INFO("Finding missing networks");

    std::vector<std::string> missingNetworks;

    // Get all networks
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return missingNetworks;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/networks";
#else
    std::string url = "http://localhost/v1.41/networks";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to fetch networks");
        return missingNetworks;
    }

    // For now, we assume all required networks exist
    // A more sophisticated check would parse docker-compose files
    // or configuration files to determine expected networks

    return missingNetworks;
}

std::vector<std::string> DependencyResolver::findMissingVolumes() {
    LOG_INFO("Finding missing volumes");

    std::vector<std::string> missingVolumes;

    // Get all volumes
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return missingVolumes;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/volumes";
#else
    std::string url = "http://localhost/v1.41/volumes";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to fetch volumes");
        return missingVolumes;
    }

    // Similar to networks, assume all required volumes exist
    // unless we parse configuration files

    return missingVolumes;
}

StartupOrder DependencyResolver::calculateStartupOrder() {
    LOG_INFO("Calculating startup order");

    StartupOrder order;
    order.hasCircularDependency = false;

    auto graph = buildDependencyGraph();

    // Check for circular dependencies
    std::set<std::string> visited;
    std::set<std::string> recursionStack;

    for (const auto& [node, deps] : graph.dependencies) {
        if (visited.find(node) == visited.end()) {
            if (detectCycle(node, visited, recursionStack, graph.dependencies)) {
                order.hasCircularDependency = true;
                order.circularPath = std::vector<std::string>(recursionStack.begin(), recursionStack.end());
                return order;
            }
        }
    }

    // Perform topological sort using Kahn's algorithm
    std::map<std::string, int> inDegree;
    std::queue<std::string> zeroInDegree;

    // Calculate in-degrees
    for (const auto& [node, deps] : graph.dependencies) {
        if (inDegree.find(node) == inDegree.end()) {
            inDegree[node] = 0;
        }
        for (const auto& dep : deps) {
            inDegree[dep]++;
        }
    }

    // Find nodes with zero in-degree
    for (const auto& [node, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegree.push(node);
        }
    }

    // Process layers
    while (!zeroInDegree.empty()) {
        std::vector<std::string> layer;
        int layerSize = zeroInDegree.size();

        for (int i = 0; i < layerSize; i++) {
            std::string node = zeroInDegree.front();
            zeroInDegree.pop();
            layer.push_back(node);

            // Reduce in-degree of neighbors
            if (graph.dependencies.find(node) != graph.dependencies.end()) {
                for (const auto& neighbor : graph.dependencies[node]) {
                    inDegree[neighbor]--;
                    if (inDegree[neighbor] == 0) {
                        zeroInDegree.push(neighbor);
                    }
                }
            }
        }

        order.layers.push_back(layer);
    }

    return order;
}

std::vector<std::string> DependencyResolver::getStartupOrder(const std::string& containerId) {
    LOG_INFO("Getting startup order for container: " + containerId);

    std::vector<std::string> startupOrder;
    auto graph = buildDependencyGraph();

    // Get all dependencies of this container (transitive)
    std::set<std::string> visited;
    std::queue<std::string> toVisit;
    toVisit.push(containerId);

    while (!toVisit.empty()) {
        std::string current = toVisit.front();
        toVisit.pop();

        if (visited.find(current) != visited.end()) {
            continue;
        }
        visited.insert(current);

        if (graph.dependencies.find(current) != graph.dependencies.end()) {
            for (const auto& dep : graph.dependencies[current]) {
                toVisit.push(dep);
            }
        }
    }

    // Perform topological sort on visited nodes
    std::set<std::string> sortVisited;
    std::vector<std::string> stack;

    for (const auto& node : visited) {
        if (sortVisited.find(node) == sortVisited.end()) {
            topologicalSort(node, sortVisited, stack, graph.dependencies);
        }
    }

    // Reverse to get correct order
    std::reverse(stack.begin(), stack.end());
    return stack;
}

bool DependencyResolver::hasCircularDependencies() {
    LOG_INFO("Checking for circular dependencies");

    auto graph = buildDependencyGraph();
    std::set<std::string> visited;
    std::set<std::string> recursionStack;

    for (const auto& [node, deps] : graph.dependencies) {
        if (visited.find(node) == visited.end()) {
            if (detectCycle(node, visited, recursionStack, graph.dependencies)) {
                return true;
            }
        }
    }

    return false;
}

bool DependencyResolver::resolveDependencies(const std::string& containerId) {
    LOG_INFO("Resolving dependencies for container: " + containerId);

    bool success = true;

    success &= pullMissingImages(containerId);
    success &= createMissingNetworks(containerId);
    success &= createMissingVolumes(containerId);

    return success;
}

bool DependencyResolver::pullMissingImages(const std::string& containerId) {
    LOG_INFO("Pulling missing images for container: " + containerId);

    auto deps = analyzeDependencies(containerId);

    for (const auto& dep : deps) {
        if (dep.type == DependencyType::ImageBase && !dep.isSatisfied) {
            LOG_INFO("Pulling image: " + dep.details);

            CURL* curl = curl_easy_init();
            if (!curl) {
                return false;
            }

            std::string response;
            std::string encodedImage = dep.details;
            // URL encode the image name
            std::replace(encodedImage.begin(), encodedImage.end(), '/', '%2F');

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
                LOG_ERROR("Failed to pull image: " + dep.details);
                return false;
            }
        }
    }

    return true;
}

bool DependencyResolver::createMissingNetworks(const std::string& containerId) {
    LOG_INFO("Creating missing networks for container: " + containerId);

    // Networks are typically created by Docker Compose or manually
    // We can create basic networks if needed

    return true;
}

bool DependencyResolver::createMissingVolumes(const std::string& containerId) {
    LOG_INFO("Creating missing volumes for container: " + containerId);

    // Volumes are typically created automatically by Docker
    // or defined in Docker Compose

    return true;
}

std::vector<std::string> DependencyResolver::findAffectedContainers(const std::string& containerId) {
    LOG_INFO("Finding affected containers for: " + containerId);

    auto graph = buildDependencyGraph();
    std::vector<std::string> affected;

    // Find all containers that depend on this one
    if (graph.dependents.find(containerId) != graph.dependents.end()) {
        affected = graph.dependents[containerId];
    }

    return affected;
}

std::vector<std::string> DependencyResolver::findContainersThatDependOn(const std::string& containerId) {
    LOG_INFO("Finding containers that depend on: " + containerId);

    return findAffectedContainers(containerId);
}

bool DependencyResolver::canSafelyRemove(const std::string& containerId) {
    LOG_INFO("Checking if container can be safely removed: " + containerId);

    auto affected = findAffectedContainers(containerId);
    return affected.empty();
}

bool DependencyResolver::detectCycle(const std::string& node,
                                     std::set<std::string>& visited,
                                     std::set<std::string>& recursionStack,
                                     const std::map<std::string, std::vector<std::string>>& graph) {
    visited.insert(node);
    recursionStack.insert(node);

    if (graph.find(node) != graph.end()) {
        for (const auto& neighbor : graph.at(node)) {
            if (visited.find(neighbor) == visited.end()) {
                if (detectCycle(neighbor, visited, recursionStack, graph)) {
                    return true;
                }
            } else if (recursionStack.find(neighbor) != recursionStack.end()) {
                return true; // Cycle detected
            }
        }
    }

    recursionStack.erase(node);
    return false;
}

void DependencyResolver::topologicalSort(const std::string& node,
                                        std::set<std::string>& visited,
                                        std::vector<std::string>& stack,
                                        const std::map<std::string, std::vector<std::string>>& graph) {
    visited.insert(node);

    if (graph.find(node) != graph.end()) {
        for (const auto& neighbor : graph.at(node)) {
            if (visited.find(neighbor) == visited.end()) {
                topologicalSort(neighbor, visited, stack, graph);
            }
        }
    }

    stack.push_back(node);
}

} // namespace docker
