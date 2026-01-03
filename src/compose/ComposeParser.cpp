#include "compose/ComposeParser.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace compose {

ComposeParser::ComposeParser() = default;
ComposeParser::~ComposeParser() = default;

std::optional<ComposeFile> ComposeParser::parseFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            lastError_ = "Failed to open file: " + filePath;
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return parseContent(buffer.str());
    } catch (const std::exception& e) {
        lastError_ = "Error reading file: " + std::string(e.what());
        return std::nullopt;
    }
}

std::optional<ComposeFile> ComposeParser::parseContent(const std::string& content) {
    try {
        YAML::Node root = YAML::Load(content);
        ComposeFile compose;

        // Parse version
        if (root["version"]) {
            compose.version = root["version"].as<std::string>();
        }

        // Parse project name
        if (root["name"]) {
            compose.name = root["name"].as<std::string>();
        }

        // Parse services
        if (root["services"]) {
            parseServices(&root["services"], compose);
        }

        // Parse networks
        if (root["networks"]) {
            parseNetworks(&root["networks"], compose);
        }

        // Parse volumes
        if (root["volumes"]) {
            parseVolumes(&root["volumes"], compose);
        }

        return compose;
    } catch (const YAML::Exception& e) {
        lastError_ = "YAML parsing error: " + std::string(e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        lastError_ = "Error parsing compose file: " + std::string(e.what());
        return std::nullopt;
    }
}

void ComposeParser::parseServices(const void* nodePtr, ComposeFile& compose) {
    const YAML::Node& servicesNode = *static_cast<const YAML::Node*>(nodePtr);

    for (const auto& item : servicesNode) {
        std::string serviceName = item.first.as<std::string>();
        Service service = parseService(serviceName, &item.second);
        compose.services.push_back(service);
    }
}

Service ComposeParser::parseService(const std::string& name, const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    Service service;
    service.name = name;

    // Image
    if (node["image"]) {
        service.image = node["image"].as<std::string>();
    }

    // Container name
    if (node["container_name"]) {
        service.containerName = node["container_name"].as<std::string>();
    }

    // Build
    if (node["build"]) {
        if (node["build"].IsScalar()) {
            service.build = node["build"].as<std::string>();
        } else if (node["build"]["context"]) {
            service.build = node["build"]["context"].as<std::string>();
        }
    }

    // Command
    if (node["command"]) {
        if (node["command"].IsSequence()) {
            service.command = node["command"].as<std::vector<std::string>>();
        } else {
            service.command.push_back(node["command"].as<std::string>());
        }
    }

    // Entrypoint
    if (node["entrypoint"]) {
        if (node["entrypoint"].IsSequence()) {
            service.entrypoint = node["entrypoint"].as<std::vector<std::string>>();
        } else {
            service.entrypoint.push_back(node["entrypoint"].as<std::string>());
        }
    }

    // Environment
    if (node["environment"]) {
        service.environment = parseEnvironment(&node["environment"]);
    }

    // Env files
    if (node["env_file"]) {
        if (node["env_file"].IsSequence()) {
            service.envFiles = node["env_file"].as<std::vector<std::string>>();
        } else {
            service.envFiles.push_back(node["env_file"].as<std::string>());
        }
    }

    // Ports
    if (node["ports"]) {
        service.ports = parsePorts(&node["ports"]);
    }

    // Volumes
    if (node["volumes"]) {
        service.volumes = parseVolumeMounts(&node["volumes"]);
    }

    // Depends on
    if (node["depends_on"]) {
        if (node["depends_on"].IsSequence()) {
            service.dependsOn = node["depends_on"].as<std::vector<std::string>>();
        } else if (node["depends_on"].IsMap()) {
            for (const auto& dep : node["depends_on"]) {
                service.dependsOn.push_back(dep.first.as<std::string>());
            }
        }
    }

    // Networks
    if (node["networks"]) {
        service.networks = parseServiceNetworks(&node["networks"]);
    }

    // Restart policy
    if (node["restart"]) {
        service.restart = node["restart"].as<std::string>();
    }

    // Health check
    if (node["healthcheck"]) {
        service.healthCheck = parseHealthCheck(&node["healthcheck"]);
    }

    // Resources (deploy section)
    if (node["deploy"] && node["deploy"]["resources"]) {
        service.resources = parseResources(&node["deploy"]["resources"]);
    }

    // Labels
    if (node["labels"]) {
        if (node["labels"].IsMap()) {
            for (const auto& label : node["labels"]) {
                service.labels[label.first.as<std::string>()] = label.second.as<std::string>();
            }
        }
    }

    // Privileged
    if (node["privileged"]) {
        service.privileged = node["privileged"].as<bool>();
    }

    // Working directory
    if (node["working_dir"]) {
        service.workingDir = node["working_dir"].as<std::string>();
    }

    // User
    if (node["user"]) {
        service.user = node["user"].as<std::string>();
    }

    // Capabilities
    if (node["cap_add"]) {
        service.capAdd = node["cap_add"].as<std::vector<std::string>>();
    }
    if (node["cap_drop"]) {
        service.capDrop = node["cap_drop"].as<std::vector<std::string>>();
    }

    // DNS
    if (node["dns"]) {
        if (node["dns"].IsSequence()) {
            service.dns = node["dns"].as<std::vector<std::string>>();
        } else {
            service.dns.push_back(node["dns"].as<std::string>());
        }
    }

    // Extra hosts
    if (node["extra_hosts"]) {
        service.extraHosts = node["extra_hosts"].as<std::vector<std::string>>();
    }

    return service;
}

std::vector<PortMapping> ComposeParser::parsePorts(const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    std::vector<PortMapping> ports;

    for (const auto& item : node) {
        PortMapping mapping;
        std::string portStr = item.as<std::string>();

        // Parse "HOST:CONTAINER" or "CONTAINER" or "HOST:CONTAINER/PROTOCOL"
        size_t colonPos = portStr.find(':');
        size_t slashPos = portStr.find('/');

        if (slashPos != std::string::npos) {
            mapping.protocol = portStr.substr(slashPos + 1);
            portStr = portStr.substr(0, slashPos);
        }

        if (colonPos != std::string::npos) {
            mapping.host = portStr.substr(0, colonPos);
            mapping.container = portStr.substr(colonPos + 1);
        } else {
            mapping.host = portStr;
            mapping.container = portStr;
        }

        ports.push_back(mapping);
    }

    return ports;
}

std::vector<VolumeMount> ComposeParser::parseVolumeMounts(const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    std::vector<VolumeMount> volumes;

    for (const auto& item : node) {
        VolumeMount mount;

        if (item.IsScalar()) {
            std::string volStr = item.as<std::string>();
            size_t colonPos = volStr.find(':');

            if (colonPos != std::string::npos) {
                mount.source = volStr.substr(0, colonPos);
                std::string rest = volStr.substr(colonPos + 1);

                size_t modePos = rest.find(':');
                if (modePos != std::string::npos) {
                    mount.target = rest.substr(0, modePos);
                    mount.mode = rest.substr(modePos + 1);
                } else {
                    mount.target = rest;
                }

                // Determine if it's a named volume or bind mount
                mount.isNamedVolume = (mount.source[0] != '/' && mount.source[0] != '.');
            } else {
                mount.target = volStr;
                mount.isNamedVolume = false;
            }
        } else if (item.IsMap()) {
            if (item["source"]) mount.source = item["source"].as<std::string>();
            if (item["target"]) mount.target = item["target"].as<std::string>();
            if (item["read_only"]) {
                mount.mode = item["read_only"].as<bool>() ? "ro" : "rw";
            }
            if (item["type"]) {
                mount.isNamedVolume = (item["type"].as<std::string>() == "volume");
            }
        }

        volumes.push_back(mount);
    }

    return volumes;
}

std::vector<EnvVariable> ComposeParser::parseEnvironment(const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    std::vector<EnvVariable> env;

    if (node.IsMap()) {
        for (const auto& item : node) {
            EnvVariable var;
            var.key = item.first.as<std::string>();
            var.value = item.second.as<std::string>();
            env.push_back(var);
        }
    } else if (node.IsSequence()) {
        for (const auto& item : node) {
            std::string envStr = item.as<std::string>();
            size_t eqPos = envStr.find('=');

            EnvVariable var;
            if (eqPos != std::string::npos) {
                var.key = envStr.substr(0, eqPos);
                var.value = envStr.substr(eqPos + 1);
            } else {
                var.key = envStr;
                var.value = "";
            }
            env.push_back(var);
        }
    }

    return env;
}

std::vector<NetworkConfig> ComposeParser::parseServiceNetworks(const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    std::vector<NetworkConfig> networks;

    if (node.IsSequence()) {
        for (const auto& item : node) {
            NetworkConfig config;
            config.name = item.as<std::string>();
            networks.push_back(config);
        }
    } else if (node.IsMap()) {
        for (const auto& item : node) {
            NetworkConfig config;
            config.name = item.first.as<std::string>();

            if (item.second["aliases"]) {
                config.aliases = item.second["aliases"].as<std::vector<std::string>>();
            }
            if (item.second["ipv4_address"]) {
                config.ipv4Address = item.second["ipv4_address"].as<std::string>();
            }
            if (item.second["ipv6_address"]) {
                config.ipv6Address = item.second["ipv6_address"].as<std::string>();
            }

            networks.push_back(config);
        }
    }

    return networks;
}

std::optional<HealthCheck> ComposeParser::parseHealthCheck(const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    HealthCheck healthCheck;

    if (node["test"]) {
        if (node["test"].IsSequence()) {
            healthCheck.test = node["test"].as<std::vector<std::string>>();
        } else {
            healthCheck.test.push_back(node["test"].as<std::string>());
        }
    }

    if (node["interval"]) {
        std::string interval = node["interval"].as<std::string>();
        // Parse duration like "30s" to seconds
        healthCheck.interval = std::stoi(interval);
    }

    if (node["timeout"]) {
        std::string timeout = node["timeout"].as<std::string>();
        healthCheck.timeout = std::stoi(timeout);
    }

    if (node["retries"]) {
        healthCheck.retries = node["retries"].as<int>();
    }

    if (node["start_period"]) {
        std::string startPeriod = node["start_period"].as<std::string>();
        healthCheck.startPeriod = std::stoi(startPeriod);
    }

    return healthCheck;
}

std::optional<ResourceLimits> ComposeParser::parseResources(const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    ResourceLimits resources;

    if (node["limits"]) {
        if (node["limits"]["cpus"]) {
            resources.cpus = node["limits"]["cpus"].as<std::string>();
        }
        if (node["limits"]["memory"]) {
            resources.memory = node["limits"]["memory"].as<std::string>();
        }
    }

    return resources;
}

void ComposeParser::parseNetworks(const void* nodePtr, ComposeFile& compose) {
    const YAML::Node& networksNode = *static_cast<const YAML::Node*>(nodePtr);

    for (const auto& item : networksNode) {
        std::string networkName = item.first.as<std::string>();
        Network network = parseNetwork(networkName, &item.second);
        compose.networks.push_back(network);
    }
}

Network ComposeParser::parseNetwork(const std::string& name, const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    Network network;
    network.name = name;

    if (node["driver"]) {
        network.driver = node["driver"].as<std::string>();
    }

    if (node["external"]) {
        network.external = node["external"].as<bool>();
    }

    if (node["driver_opts"]) {
        for (const auto& opt : node["driver_opts"]) {
            network.driverOpts[opt.first.as<std::string>()] = opt.second.as<std::string>();
        }
    }

    if (node["labels"]) {
        for (const auto& label : node["labels"]) {
            network.labels[label.first.as<std::string>()] = label.second.as<std::string>();
        }
    }

    return network;
}

void ComposeParser::parseVolumes(const void* nodePtr, ComposeFile& compose) {
    const YAML::Node& volumesNode = *static_cast<const YAML::Node*>(nodePtr);

    for (const auto& item : volumesNode) {
        std::string volumeName = item.first.as<std::string>();
        Volume volume = parseVolume(volumeName, &item.second);
        compose.volumes.push_back(volume);
    }
}

Volume ComposeParser::parseVolume(const std::string& name, const void* nodePtr) {
    const YAML::Node& node = *static_cast<const YAML::Node*>(nodePtr);
    Volume volume;
    volume.name = name;

    if (node.IsNull()) {
        return volume;
    }

    if (node["driver"]) {
        volume.driver = node["driver"].as<std::string>();
    }

    if (node["external"]) {
        volume.external = node["external"].as<bool>();
    }

    if (node["driver_opts"]) {
        for (const auto& opt : node["driver_opts"]) {
            volume.driverOpts[opt.first.as<std::string>()] = opt.second.as<std::string>();
        }
    }

    if (node["labels"]) {
        for (const auto& label : node["labels"]) {
            volume.labels[label.first.as<std::string>()] = label.second.as<std::string>();
        }
    }

    return volume;
}

std::vector<std::string> ComposeParser::validate(const ComposeFile& compose) {
    std::vector<std::string> errors;

    // Check for services
    if (compose.services.empty()) {
        errors.push_back("No services defined");
    }

    // Validate each service
    for (const auto& service : compose.services) {
        if (service.name.empty()) {
            errors.push_back("Service with empty name found");
        }

        if (service.image.empty() && !service.build.has_value()) {
            errors.push_back("Service '" + service.name + "' has no image or build specification");
        }

        // Check dependency references
        for (const auto& dep : service.dependsOn) {
            bool found = false;
            for (const auto& otherService : compose.services) {
                if (otherService.name == dep) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                errors.push_back("Service '" + service.name + "' depends on unknown service '" + dep + "'");
            }
        }

        // Check network references
        for (const auto& netConfig : service.networks) {
            if (!compose.networks.empty()) {
                bool found = false;
                for (const auto& net : compose.networks) {
                    if (net.name == netConfig.name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    errors.push_back("Service '" + service.name + "' references unknown network '" + netConfig.name + "'");
                }
            }
        }

        // Check volume references
        for (const auto& volMount : service.volumes) {
            if (volMount.isNamedVolume && !compose.volumes.empty()) {
                bool found = false;
                for (const auto& vol : compose.volumes) {
                    if (vol.name == volMount.source) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    errors.push_back("Service '" + service.name + "' references unknown volume '" + volMount.source + "'");
                }
            }
        }
    }

    return errors;
}

std::string ComposeParser::getLastError() const {
    return lastError_;
}

std::string ComposeParser::toYaml(const ComposeFile& compose) {
    YAML::Emitter out;
    out << YAML::BeginMap;

    // Version
    if (!compose.version.empty()) {
        out << YAML::Key << "version" << YAML::Value << compose.version;
    }

    // Name
    if (!compose.name.empty()) {
        out << YAML::Key << "name" << YAML::Value << compose.name;
    }

    // Services
    if (!compose.services.empty()) {
        out << YAML::Key << "services" << YAML::Value << YAML::BeginMap;
        for (const auto& service : compose.services) {
            out << YAML::Key << service.name << YAML::Value << YAML::BeginMap;

            if (!service.image.empty()) {
                out << YAML::Key << "image" << YAML::Value << service.image;
            }

            if (service.containerName.has_value()) {
                out << YAML::Key << "container_name" << YAML::Value << *service.containerName;
            }

            // Add other fields as needed...

            out << YAML::EndMap;
        }
        out << YAML::EndMap;
    }

    // Networks
    if (!compose.networks.empty()) {
        out << YAML::Key << "networks" << YAML::Value << YAML::BeginMap;
        for (const auto& network : compose.networks) {
            out << YAML::Key << network.name << YAML::Value << YAML::BeginMap;
            if (network.driver.has_value()) {
                out << YAML::Key << "driver" << YAML::Value << *network.driver;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndMap;
    }

    // Volumes
    if (!compose.volumes.empty()) {
        out << YAML::Key << "volumes" << YAML::Value << YAML::BeginMap;
        for (const auto& volume : compose.volumes) {
            out << YAML::Key << volume.name << YAML::Value << YAML::BeginMap;
            out << YAML::EndMap;
        }
        out << YAML::EndMap;
    }

    out << YAML::EndMap;

    return out.c_str();
}

} // namespace compose
