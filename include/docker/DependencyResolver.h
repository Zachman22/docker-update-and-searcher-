#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace docker {

enum class DependencyType {
    Network,        // Container depends on being on same network
    VolumeMount,    // Container depends on shared volume
    Link,           // Legacy container links
    DependsOn,      // Docker Compose depends_on
    PortConflict,   // Container needs port to be available
    ImageBase       // Container depends on base image
};

struct Dependency {
    std::string sourceContainer;
    std::string targetContainer;
    DependencyType type;
    std::string details; // Additional info (network name, volume name, etc.)
    bool isSatisfied;
    std::string unsatisfiedReason;
};

struct DependencyGraph {
    std::map<std::string, std::vector<std::string>> dependencies; // container -> list of dependencies
    std::map<std::string, std::vector<std::string>> dependents;   // container -> list of dependents
};

struct StartupOrder {
    std::vector<std::vector<std::string>> layers; // Each layer can start in parallel
    bool hasCircularDependency;
    std::vector<std::string> circularPath;
};

class DependencyResolver {
public:
    DependencyResolver();
    ~DependencyResolver();

    // Dependency discovery
    std::vector<Dependency> analyzeDependencies(const std::string& containerId);
    std::vector<Dependency> findAllDependencies();
    DependencyGraph buildDependencyGraph();

    // Dependency checking
    bool areDependenciesSatisfied(const std::string& containerId);
    std::vector<std::string> findUnsatisfiedDependencies(const std::string& containerId);
    std::vector<std::string> findMissingImages();
    std::vector<std::string> findMissingNetworks();
    std::vector<std::string> findMissingVolumes();

    // Startup order
    StartupOrder calculateStartupOrder();
    std::vector<std::string> getStartupOrder(const std::string& containerId);
    bool hasCircularDependencies();

    // Auto-resolution
    bool resolveDependencies(const std::string& containerId);
    bool pullMissingImages(const std::string& containerId);
    bool createMissingNetworks(const std::string& containerId);
    bool createMissingVolumes(const std::string& containerId);

    // Impact analysis
    std::vector<std::string> findAffectedContainers(const std::string& containerId);
    std::vector<std::string> findContainersThatDependOn(const std::string& containerId);
    bool canSafelyRemove(const std::string& containerId);

private:
    bool detectCycle(const std::string& node,
                     std::set<std::string>& visited,
                     std::set<std::string>& recursionStack,
                     const std::map<std::string, std::vector<std::string>>& graph);

    void topologicalSort(const std::string& node,
                        std::set<std::string>& visited,
                        std::vector<std::string>& stack,
                        const std::map<std::string, std::vector<std::string>>& graph);
};

} // namespace docker
