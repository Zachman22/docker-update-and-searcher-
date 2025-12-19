#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace network {

struct PortInfo {
    int port;
    std::string protocol; // "tcp" or "udp"
    bool isOpen;
    std::string processName;
    int processId;
    std::string containerId;
    std::string containerName;
};

struct PortConflict {
    int port;
    std::string protocol;
    std::vector<PortInfo> conflictingProcesses;
    std::string description;
    std::vector<std::string> suggestedFixes;
};

class PortScanner {
public:
    PortScanner();
    ~PortScanner();

    // Port scanning
    std::vector<PortInfo> scanOpenPorts();
    std::vector<PortInfo> scanPortRange(int startPort, int endPort);
    bool isPortOpen(int port, const std::string& protocol = "tcp");
    std::optional<PortInfo> getPortInfo(int port, const std::string& protocol = "tcp");

    // Conflict detection
    std::vector<PortConflict> detectConflicts();
    std::vector<PortConflict> detectConflictsForContainer(const std::string& containerId);
    bool wouldCauseConflict(int port, const std::string& protocol = "tcp");

    // Port management
    std::vector<int> findAvailablePorts(int count, int startPort = 8000, int endPort = 9000);
    int findNextAvailablePort(int startPort = 8000);
    std::map<std::string, std::vector<PortInfo>> getPortsByContainer();

    // Suggestions
    int suggestAlternativePort(int conflictingPort);
    std::vector<std::string> generateFixSuggestions(const PortConflict& conflict);

private:
    std::vector<PortInfo> cachedPorts_;

    // Platform-specific implementations
    std::vector<PortInfo> scanOpenPortsWindows();
    std::vector<PortInfo> scanOpenPortsLinux();
    std::vector<PortInfo> scanOpenPortsMacOS();

    PortInfo getProcessInfo(int port, const std::string& protocol);
    std::string getContainerIdFromProcess(int processId);
};

} // namespace network
