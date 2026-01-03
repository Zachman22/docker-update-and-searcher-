#include "compose/ComposeStack.h"
#include <filesystem>
#include <algorithm>
#include <queue>
#include <set>

namespace compose {

ComposeStack::ComposeStack(std::shared_ptr<docker::DockerClient> dockerClient)
    : dockerClient_(dockerClient) {}

ComposeStack::~ComposeStack() = default;

bool ComposeStack::loadStack(const std::string& filePath, const std::string& stackName) {
    auto composeOpt = parser_.parseFile(filePath);
    if (!composeOpt.has_value()) {
        lastError_ = "Failed to parse compose file: " + parser_.getLastError();
        return false;
    }

    std::string name = stackName;
    if (name.empty()) {
        // Use parent directory name as stack name
        std::filesystem::path path(filePath);
        name = path.parent_path().filename().string();
        if (name.empty()) {
            name = "default";
        }
    }

    if (composeOpt->name.empty()) {
        composeOpt->name = name;
    }

    stacks_[name] = *composeOpt;
    stackPaths_[name] = filePath;

    return true;
}

bool ComposeStack::createStackFromContent(const std::string& content, const std::string& stackName) {
    auto composeOpt = parser_.parseContent(content);
    if (!composeOpt.has_value()) {
        lastError_ = "Failed to parse compose content: " + parser_.getLastError();
        return false;
    }

    if (composeOpt->name.empty()) {
        composeOpt->name = stackName;
    }

    stacks_[stackName] = *composeOpt;
    return true;
}

std::string ComposeStack::getProjectName(const std::string& stackName) {
    auto it = stacks_.find(stackName);
    if (it != stacks_.end() && !it->second.name.empty()) {
        return it->second.name;
    }
    return stackName;
}

bool ComposeStack::createNetworks(const ComposeFile& compose, const std::string& projectName) {
    for (const auto& network : compose.networks) {
        if (network.external) {
            continue; // Don't create external networks
        }

        std::string networkName = projectName + "_" + network.name;

        // Check if network already exists
        auto networks = dockerClient_->listNetworks();
        bool exists = false;
        for (const auto& net : networks) {
            if (net.name == networkName) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            std::string driver = network.driver.value_or("bridge");
            if (!dockerClient_->createNetwork(networkName, driver)) {
                lastError_ = "Failed to create network: " + networkName;
                return false;
            }
        }
    }
    return true;
}

bool ComposeStack::createVolumes(const ComposeFile& compose, const std::string& projectName) {
    for (const auto& volume : compose.volumes) {
        if (volume.external) {
            continue; // Don't create external volumes
        }

        std::string volumeName = projectName + "_" + volume.name;

        // Check if volume already exists
        auto volumes = dockerClient_->listVolumes();
        bool exists = false;
        for (const auto& vol : volumes) {
            if (vol.name == volumeName) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            std::string driver = volume.driver.value_or("local");
            if (!dockerClient_->createVolume(volumeName, driver)) {
                lastError_ = "Failed to create volume: " + volumeName;
                return false;
            }
        }
    }
    return true;
}

std::string ComposeStack::getContainerNameForService(const std::string& stackName, const Service& service) {
    if (service.containerName.has_value()) {
        return *service.containerName;
    }
    return getProjectName(stackName) + "_" + service.name + "_1";
}

bool ComposeStack::createService(const Service& service, const std::string& projectName, bool recreate) {
    std::string containerName = service.containerName.value_or(projectName + "_" + service.name + "_1");

    // Check if container already exists
    auto containers = dockerClient_->listContainers(true); // Include stopped
    std::string existingId;
    for (const auto& container : containers) {
        for (const auto& name : container.names) {
            if (name == "/" + containerName || name == containerName) {
                existingId = container.id;
                break;
            }
        }
        if (!existingId.empty()) break;
    }

    if (!existingId.empty() && !recreate) {
        // Container exists, just start it
        return dockerClient_->startContainer(existingId);
    }

    if (!existingId.empty() && recreate) {
        // Remove existing container
        dockerClient_->stopContainer(existingId);
        dockerClient_->removeContainer(existingId, false, true);
    }

    // Pull image if needed
    if (!service.image.empty()) {
        auto images = dockerClient_->listImages();
        bool imageExists = false;
        for (const auto& img : images) {
            for (const auto& tag : img.repoTags) {
                if (tag == service.image) {
                    imageExists = true;
                    break;
                }
            }
            if (imageExists) break;
        }

        if (!imageExists) {
            if (!dockerClient_->pullImage(service.image)) {
                lastError_ = "Failed to pull image: " + service.image;
                return false;
            }
        }
    }

    // Build container configuration
    nlohmann::json config = {
        {"Image", service.image},
        {"Hostname", containerName}
    };

    if (service.containerName.has_value()) {
        config["Name"] = *service.containerName;
    }

    // Environment variables
    if (!service.environment.empty()) {
        std::vector<std::string> env;
        for (const auto& var : service.environment) {
            env.push_back(var.key + "=" + var.value);
        }
        config["Env"] = env;
    }

    // Command
    if (!service.command.empty()) {
        config["Cmd"] = service.command;
    }

    // Entrypoint
    if (!service.entrypoint.empty()) {
        config["Entrypoint"] = service.entrypoint;
    }

    // Working directory
    if (service.workingDir.has_value()) {
        config["WorkingDir"] = *service.workingDir;
    }

    // User
    if (service.user.has_value()) {
        config["User"] = *service.user;
    }

    // Labels
    if (!service.labels.empty()) {
        config["Labels"] = service.labels;
    }

    // Host configuration
    nlohmann::json hostConfig;

    // Port bindings
    if (!service.ports.empty()) {
        nlohmann::json portBindings;
        nlohmann::json exposedPorts;

        for (const auto& port : service.ports) {
            std::string containerPort = port.container + "/" + port.protocol;
            exposedPorts[containerPort] = nlohmann::json::object();

            portBindings[containerPort] = nlohmann::json::array({
                {{"HostPort", port.host}}
            });
        }

        config["ExposedPorts"] = exposedPorts;
        hostConfig["PortBindings"] = portBindings;
    }

    // Volume binds
    if (!service.volumes.empty()) {
        std::vector<std::string> binds;
        for (const auto& vol : service.volumes) {
            std::string bind;
            if (vol.isNamedVolume) {
                bind = projectName + "_" + vol.source + ":" + vol.target;
            } else {
                bind = vol.source + ":" + vol.target;
            }
            if (!vol.mode.empty()) {
                bind += ":" + vol.mode;
            }
            binds.push_back(bind);
        }
        hostConfig["Binds"] = binds;
    }

    // Restart policy
    if (service.restart.has_value()) {
        std::string policy = *service.restart;
        if (policy == "always" || policy == "unless-stopped") {
            hostConfig["RestartPolicy"] = {
                {"Name", policy}
            };
        } else if (policy == "on-failure") {
            hostConfig["RestartPolicy"] = {
                {"Name", "on-failure"},
                {"MaximumRetryCount", 5}
            };
        }
    }

    // Privileged
    if (service.privileged) {
        hostConfig["Privileged"] = true;
    }

    // Capabilities
    if (!service.capAdd.empty()) {
        hostConfig["CapAdd"] = service.capAdd;
    }
    if (!service.capDrop.empty()) {
        hostConfig["CapDrop"] = service.capDrop;
    }

    // DNS
    if (!service.dns.empty()) {
        hostConfig["Dns"] = service.dns;
    }

    // Extra hosts
    if (!service.extraHosts.empty()) {
        hostConfig["ExtraHosts"] = service.extraHosts;
    }

    config["HostConfig"] = hostConfig;

    // Create container
    auto result = dockerClient_->createContainer(config, containerName);
    if (!result.has_value()) {
        lastError_ = "Failed to create container: " + containerName;
        return false;
    }

    // Connect to networks
    for (const auto& netConfig : service.networks) {
        std::string networkName = projectName + "_" + netConfig.name;
        dockerClient_->connectContainerToNetwork(*result, networkName);
    }

    return true;
}

std::vector<std::string> ComposeStack::resolveDependencyOrder(const ComposeFile& compose) {
    std::map<std::string, std::vector<std::string>> graph;
    std::map<std::string, int> inDegree;

    // Build dependency graph
    for (const auto& service : compose.services) {
        graph[service.name] = service.dependsOn;
        inDegree[service.name] = 0;
    }

    for (const auto& service : compose.services) {
        for (const auto& dep : service.dependsOn) {
            inDegree[service.name]++;
        }
    }

    // Topological sort (Kahn's algorithm)
    std::queue<std::string> queue;
    for (const auto& [name, degree] : inDegree) {
        if (degree == 0) {
            queue.push(name);
        }
    }

    std::vector<std::string> order;
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        order.push_back(current);

        // Find all services that depend on current
        for (const auto& service : compose.services) {
            auto it = std::find(service.dependsOn.begin(), service.dependsOn.end(), current);
            if (it != service.dependsOn.end()) {
                inDegree[service.name]--;
                if (inDegree[service.name] == 0) {
                    queue.push(service.name);
                }
            }
        }
    }

    return order;
}

StackOperationResult ComposeStack::deployStack(const std::string& stackName, bool recreate) {
    StackOperationResult result;
    result.success = false;

    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        result.message = "Stack not found: " + stackName;
        return result;
    }

    const ComposeFile& compose = it->second;
    std::string projectName = getProjectName(stackName);

    // Create networks
    if (!createNetworks(compose, projectName)) {
        result.message = "Failed to create networks: " + lastError_;
        return result;
    }

    // Create volumes
    if (!createVolumes(compose, projectName)) {
        result.message = "Failed to create volumes: " + lastError_;
        return result;
    }

    // Get dependency order
    auto order = resolveDependencyOrder(compose);

    // Create and start services in order
    for (const auto& serviceName : order) {
        // Find service
        const Service* service = nullptr;
        for (const auto& svc : compose.services) {
            if (svc.name == serviceName) {
                service = &svc;
                break;
            }
        }

        if (!service) continue;

        // Create service
        if (createService(*service, projectName, recreate)) {
            // Start service
            std::string containerName = getContainerNameForService(stackName, *service);
            auto containers = dockerClient_->listContainers(true);
            std::string containerId;

            for (const auto& container : containers) {
                for (const auto& name : container.names) {
                    if (name == "/" + containerName || name == containerName) {
                        containerId = container.id;
                        break;
                    }
                }
                if (!containerId.empty()) break;
            }

            if (!containerId.empty()) {
                if (dockerClient_->startContainer(containerId)) {
                    result.successfulServices.push_back(serviceName);
                } else {
                    result.failedServices.push_back(serviceName);
                    result.errors[serviceName] = "Failed to start container";
                }
            }
        } else {
            result.failedServices.push_back(serviceName);
            result.errors[serviceName] = lastError_;
        }
    }

    result.success = result.failedServices.empty();
    result.message = result.success ? "Stack deployed successfully" :
                    "Stack deployed with errors: " + std::to_string(result.failedServices.size()) + " services failed";

    return result;
}

StackOperationResult ComposeStack::stopStack(const std::string& stackName, bool removeContainers) {
    StackOperationResult result;
    result.success = false;

    auto containers = getStackContainers(stackName);
    if (containers.empty()) {
        result.message = "No containers found for stack: " + stackName;
        result.success = true;
        return result;
    }

    for (const auto& container : containers) {
        if (dockerClient_->stopContainer(container.id)) {
            result.successfulServices.push_back(container.names[0]);

            if (removeContainers) {
                dockerClient_->removeContainer(container.id, false, true);
            }
        } else {
            result.failedServices.push_back(container.names[0]);
        }
    }

    result.success = result.failedServices.empty();
    result.message = result.success ? "Stack stopped successfully" :
                    "Stack stopped with errors";

    return result;
}

StackOperationResult ComposeStack::removeStack(const std::string& stackName, bool removeVolumes) {
    StackOperationResult result;

    // First stop all containers
    auto stopResult = stopStack(stackName, true);

    auto it = stacks_.find(stackName);
    if (it != stacks_.end()) {
        std::string projectName = getProjectName(stackName);

        // Remove networks
        for (const auto& network : it->second.networks) {
            if (!network.external) {
                std::string networkName = projectName + "_" + network.name;
                dockerClient_->removeNetwork(networkName);
            }
        }

        // Remove volumes if requested
        if (removeVolumes) {
            for (const auto& volume : it->second.volumes) {
                if (!volume.external) {
                    std::string volumeName = projectName + "_" + volume.name;
                    dockerClient_->removeVolume(volumeName);
                }
            }
        }

        // Remove from managed stacks
        stacks_.erase(stackName);
        stackPaths_.erase(stackName);
    }

    result.success = true;
    result.message = "Stack removed successfully";
    return result;
}

StackOperationResult ComposeStack::restartStack(const std::string& stackName) {
    auto stopResult = stopStack(stackName, false);
    if (!stopResult.success) {
        return stopResult;
    }

    return deployStack(stackName, false);
}

StackOperationResult ComposeStack::updateStack(const std::string& stackName, bool pullImages) {
    StackOperationResult result;

    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        result.success = false;
        result.message = "Stack not found";
        return result;
    }

    if (pullImages) {
        // Pull all images
        for (const auto& service : it->second.services) {
            if (!service.image.empty()) {
                dockerClient_->pullImage(service.image);
            }
        }
    }

    // Recreate all containers
    return deployStack(stackName, true);
}

std::vector<docker::Container> ComposeStack::getStackContainers(const std::string& stackName) {
    std::vector<docker::Container> stackContainers;

    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        return stackContainers;
    }

    std::string projectName = getProjectName(stackName);
    auto allContainers = dockerClient_->listContainers(true);

    for (const auto& container : allContainers) {
        // Check if container belongs to this stack
        for (const auto& service : it->second.services) {
            std::string expectedName = getContainerNameForService(stackName, service);

            for (const auto& name : container.names) {
                std::string cleanName = name;
                if (!cleanName.empty() && cleanName[0] == '/') {
                    cleanName = cleanName.substr(1);
                }

                if (cleanName == expectedName) {
                    stackContainers.push_back(container);
                    break;
                }
            }
        }
    }

    return stackContainers;
}

ServiceStatus ComposeStack::getServiceStatus(const std::string& stackName, const Service& service) {
    ServiceStatus status;
    status.name = service.name;
    status.isRunning = false;

    std::string expectedName = getContainerNameForService(stackName, service);
    auto containers = dockerClient_->listContainers(true);

    for (const auto& container : containers) {
        for (const auto& name : container.names) {
            std::string cleanName = name;
            if (!cleanName.empty() && cleanName[0] == '/') {
                cleanName = cleanName.substr(1);
            }

            if (cleanName == expectedName) {
                status.containerId = container.id;
                status.containerName = cleanName;
                status.status = container.status;
                status.isRunning = (container.state == "running");
                break;
            }
        }
    }

    return status;
}

std::optional<StackInfo> ComposeStack::getStackInfo(const std::string& stackName) {
    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        return std::nullopt;
    }

    StackInfo info;
    info.name = stackName;
    info.filePath = stackPaths_[stackName];
    info.composeFile = it->second;
    info.totalServices = static_cast<int>(it->second.services.size());
    info.runningServices = 0;

    // Get status for each service
    for (const auto& service : it->second.services) {
        auto serviceStatus = getServiceStatus(stackName, service);
        info.services.push_back(serviceStatus);

        if (serviceStatus.isRunning) {
            info.runningServices++;
        }
    }

    // Determine overall status
    if (info.runningServices == 0) {
        info.status = StackStatus::Stopped;
    } else if (info.runningServices == info.totalServices) {
        info.status = StackStatus::Running;
    } else {
        info.status = StackStatus::PartiallyUp;
    }

    return info;
}

std::vector<std::string> ComposeStack::listStacks() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : stacks_) {
        names.push_back(name);
    }
    return names;
}

std::vector<StackInfo> ComposeStack::getAllStacksInfo() {
    std::vector<StackInfo> infos;
    for (const auto& [name, _] : stacks_) {
        auto info = getStackInfo(name);
        if (info.has_value()) {
            infos.push_back(*info);
        }
    }
    return infos;
}

std::vector<std::vector<std::string>> ComposeStack::getServiceStartupOrder(const std::string& stackName) {
    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        return {};
    }

    // This would implement the layered startup order
    // For now, return simple dependency order
    auto order = resolveDependencyOrder(it->second);

    std::vector<std::vector<std::string>> layers;
    layers.push_back(order);

    return layers;
}

bool ComposeStack::exportStack(const std::string& stackName, const std::string& outputPath) {
    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        lastError_ = "Stack not found";
        return false;
    }

    std::string yaml = parser_.toYaml(it->second);

    std::ofstream file(outputPath);
    if (!file.is_open()) {
        lastError_ = "Failed to open output file";
        return false;
    }

    file << yaml;
    return true;
}

std::vector<std::string> ComposeStack::validateStack(const std::string& stackName) {
    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        return {"Stack not found"};
    }

    return parser_.validate(it->second);
}

std::string ComposeStack::getLastError() const {
    return lastError_;
}

bool ComposeStack::scaleService(const std::string& stackName, const std::string& serviceName, int replicas) {
    // TODO: Implement service scaling
    lastError_ = "Scaling not yet implemented";
    return false;
}

std::string ComposeStack::getServiceLogs(const std::string& stackName, const std::string& serviceName, int tail) {
    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        return "";
    }

    // Find service
    for (const auto& service : it->second.services) {
        if (service.name == serviceName) {
            auto status = getServiceStatus(stackName, service);
            if (!status.containerId.empty()) {
                return dockerClient_->getContainerLogs(status.containerId, tail);
            }
        }
    }

    return "";
}

std::string ComposeStack::execInService(const std::string& stackName, const std::string& serviceName,
                                       const std::vector<std::string>& command) {
    auto it = stacks_.find(stackName);
    if (it == stacks_.end()) {
        return "";
    }

    // Find service
    for (const auto& service : it->second.services) {
        if (service.name == serviceName) {
            auto status = getServiceStatus(stackName, service);
            if (!status.containerId.empty()) {
                // Execute command in container
                return dockerClient_->execInContainer(status.containerId, command);
            }
        }
    }

    return "";
}

} // namespace compose
