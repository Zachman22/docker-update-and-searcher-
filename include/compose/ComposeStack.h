#pragma once

#include "compose/ComposeParser.h"
#include "docker/DockerClient.h"
#include "docker/Container.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace compose {

// Stack status
enum class StackStatus {
    Unknown,
    Running,      // All services running
    PartiallyUp,  // Some services running
    Stopped,      // All services stopped
    Failed        // One or more services failed
};

// Service status within stack
struct ServiceStatus {
    std::string name;
    std::string containerId;
    std::string containerName;
    std::string status;
    bool isRunning;
    std::string errorMessage;
};

// Stack information
struct StackInfo {
    std::string name;
    std::string filePath;
    StackStatus status;
    std::vector<ServiceStatus> services;
    int runningServices;
    int totalServices;
    ComposeFile composeFile;
};

// Stack operation result
struct StackOperationResult {
    bool success;
    std::string message;
    std::vector<std::string> successfulServices;
    std::vector<std::string> failedServices;
    std::map<std::string, std::string> errors; // service -> error
};

/**
 * ComposeStack - Manages Docker Compose stacks as units
 */
class ComposeStack {
public:
    explicit ComposeStack(std::shared_ptr<docker::DockerClient> dockerClient);
    ~ComposeStack();

    /**
     * Load a compose file from disk
     * @param filePath Path to docker-compose.yml
     * @param stackName Optional stack name (defaults to parent directory name)
     * @return Success status
     */
    bool loadStack(const std::string& filePath, const std::string& stackName = "");

    /**
     * Create a stack from compose content
     * @param content YAML content
     * @param stackName Stack name
     * @return Success status
     */
    bool createStackFromContent(const std::string& content, const std::string& stackName);

    /**
     * Deploy/start the entire stack
     * @param stackName Stack name
     * @param recreate Force recreate containers even if they exist
     * @return Operation result
     */
    StackOperationResult deployStack(const std::string& stackName, bool recreate = false);

    /**
     * Stop the entire stack
     * @param stackName Stack name
     * @param removeContainers Remove containers after stopping
     * @return Operation result
     */
    StackOperationResult stopStack(const std::string& stackName, bool removeContainers = false);

    /**
     * Remove the entire stack (containers, networks, volumes)
     * @param stackName Stack name
     * @param removeVolumes Also remove volumes
     * @return Operation result
     */
    StackOperationResult removeStack(const std::string& stackName, bool removeVolumes = false);

    /**
     * Restart the entire stack
     * @param stackName Stack name
     * @return Operation result
     */
    StackOperationResult restartStack(const std::string& stackName);

    /**
     * Update all services in the stack
     * @param stackName Stack name
     * @param pullImages Pull latest images before updating
     * @return Operation result
     */
    StackOperationResult updateStack(const std::string& stackName, bool pullImages = true);

    /**
     * Get stack status and information
     * @param stackName Stack name
     * @return Stack information
     */
    std::optional<StackInfo> getStackInfo(const std::string& stackName);

    /**
     * List all managed stacks
     * @return Vector of stack names
     */
    std::vector<std::string> listStacks() const;

    /**
     * Get detailed information for all stacks
     * @return Vector of stack information
     */
    std::vector<StackInfo> getAllStacksInfo();

    /**
     * Scale a service within a stack
     * @param stackName Stack name
     * @param serviceName Service name
     * @param replicas Number of replicas
     * @return Success status
     */
    bool scaleService(const std::string& stackName, const std::string& serviceName, int replicas);

    /**
     * Get logs for a service
     * @param stackName Stack name
     * @param serviceName Service name
     * @param tail Number of lines to retrieve
     * @return Log content
     */
    std::string getServiceLogs(const std::string& stackName, const std::string& serviceName, int tail = 100);

    /**
     * Execute command in a service container
     * @param stackName Stack name
     * @param serviceName Service name
     * @param command Command to execute
     * @return Command output
     */
    std::string execInService(const std::string& stackName, const std::string& serviceName,
                              const std::vector<std::string>& command);

    /**
     * Validate stack configuration
     * @param stackName Stack name
     * @return Vector of validation errors
     */
    std::vector<std::string> validateStack(const std::string& stackName);

    /**
     * Get dependency order for services in stack
     * @param stackName Stack name
     * @return Ordered list of service names (layers for parallel startup)
     */
    std::vector<std::vector<std::string>> getServiceStartupOrder(const std::string& stackName);

    /**
     * Export stack configuration
     * @param stackName Stack name
     * @param outputPath Path to save the compose file
     * @return Success status
     */
    bool exportStack(const std::string& stackName, const std::string& outputPath);

    /**
     * Get last error message
     * @return Error message
     */
    std::string getLastError() const;

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    std::map<std::string, ComposeFile> stacks_;
    std::map<std::string, std::string> stackPaths_; // stackName -> filePath
    ComposeParser parser_;
    std::string lastError_;

    // Helper methods
    std::string getContainerNameForService(const std::string& stackName, const Service& service);
    std::string getProjectName(const std::string& stackName);
    bool createNetworks(const ComposeFile& compose, const std::string& projectName);
    bool createVolumes(const ComposeFile& compose, const std::string& projectName);
    bool createService(const Service& service, const std::string& projectName, bool recreate);
    bool startServiceInOrder(const Service& service, const std::string& projectName,
                            const std::map<std::string, bool>& startedServices);
    std::vector<std::string> resolveDependencyOrder(const ComposeFile& compose);
    ServiceStatus getServiceStatus(const std::string& stackName, const Service& service);
    std::vector<docker::Container> getStackContainers(const std::string& stackName);
};

} // namespace compose
