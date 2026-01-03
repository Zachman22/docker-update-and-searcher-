#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace compose {

// Port mapping structure
struct PortMapping {
    std::string host;
    std::string container;
    std::string protocol = "tcp";
};

// Volume mount structure
struct VolumeMount {
    std::string source;
    std::string target;
    std::string mode = "rw"; // rw or ro
    bool isNamedVolume = false;
};

// Environment variable
struct EnvVariable {
    std::string key;
    std::string value;
};

// Network configuration
struct NetworkConfig {
    std::string name;
    std::vector<std::string> aliases;
    std::optional<std::string> ipv4Address;
    std::optional<std::string> ipv6Address;
};

// Health check configuration
struct HealthCheck {
    std::vector<std::string> test;
    std::optional<int> interval;
    std::optional<int> timeout;
    std::optional<int> retries;
    std::optional<int> startPeriod;
};

// Resource limits
struct ResourceLimits {
    std::optional<std::string> cpus;
    std::optional<std::string> memory;
    std::optional<std::string> memswap;
    std::optional<int> pids;
};

// Service definition
struct Service {
    std::string name;
    std::string image;
    std::optional<std::string> containerName;
    std::optional<std::string> build;
    std::vector<std::string> command;
    std::vector<std::string> entrypoint;
    std::vector<EnvVariable> environment;
    std::vector<std::string> envFiles;
    std::vector<PortMapping> ports;
    std::vector<VolumeMount> volumes;
    std::vector<std::string> dependsOn;
    std::vector<NetworkConfig> networks;
    std::optional<std::string> restart;
    std::optional<HealthCheck> healthCheck;
    std::optional<ResourceLimits> resources;
    std::map<std::string, std::string> labels;
    bool privileged = false;
    std::optional<std::string> workingDir;
    std::optional<std::string> user;
    std::vector<std::string> capAdd;
    std::vector<std::string> capDrop;
    std::vector<std::string> dns;
    std::vector<std::string> extraHosts;
};

// Network definition
struct Network {
    std::string name;
    std::optional<std::string> driver;
    bool external = false;
    std::map<std::string, std::string> driverOpts;
    std::map<std::string, std::string> labels;
};

// Volume definition
struct Volume {
    std::string name;
    std::optional<std::string> driver;
    bool external = false;
    std::map<std::string, std::string> driverOpts;
    std::map<std::string, std::string> labels;
};

// Complete compose file structure
struct ComposeFile {
    std::string version;
    std::string name; // Project name
    std::vector<Service> services;
    std::vector<Network> networks;
    std::vector<Volume> volumes;
};

// Compose parser class
class ComposeParser {
public:
    ComposeParser();
    ~ComposeParser();

    /**
     * Parse a docker-compose.yml file
     * @param filePath Path to the compose file
     * @return Parsed compose file structure
     */
    std::optional<ComposeFile> parseFile(const std::string& filePath);

    /**
     * Parse compose content from string
     * @param content YAML content
     * @return Parsed compose file structure
     */
    std::optional<ComposeFile> parseContent(const std::string& content);

    /**
     * Validate a compose file structure
     * @param compose Compose file to validate
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validate(const ComposeFile& compose);

    /**
     * Get the last parse error
     * @return Error message
     */
    std::string getLastError() const;

    /**
     * Convert ComposeFile back to YAML string
     * @param compose Compose file structure
     * @return YAML string
     */
    std::string toYaml(const ComposeFile& compose);

private:
    std::string lastError_;

    // Helper parsing methods
    void parseServices(const void* node, ComposeFile& compose);
    void parseNetworks(const void* node, ComposeFile& compose);
    void parseVolumes(const void* node, ComposeFile& compose);

    Service parseService(const std::string& name, const void* node);
    Network parseNetwork(const std::string& name, const void* node);
    Volume parseVolume(const std::string& name, const void* node);

    std::vector<PortMapping> parsePorts(const void* node);
    std::vector<VolumeMount> parseVolumeMounts(const void* node);
    std::vector<EnvVariable> parseEnvironment(const void* node);
    std::vector<NetworkConfig> parseServiceNetworks(const void* node);
    std::optional<HealthCheck> parseHealthCheck(const void* node);
    std::optional<ResourceLimits> parseResources(const void* node);
};

} // namespace compose
