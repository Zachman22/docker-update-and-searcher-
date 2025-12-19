#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace network {

enum class ConnectivityStatus {
    Success,
    Failed,
    Timeout,
    DNSFailure,
    NetworkUnreachable,
    PortClosed
};

struct ConnectivityTest {
    std::string source; // Container ID or "host"
    std::string destination; // IP, hostname, or container ID
    int port;
    ConnectivityStatus status;
    std::chrono::milliseconds latency;
    std::string errorMessage;
};

struct DNSTest {
    std::string hostname;
    std::vector<std::string> resolvedIPs;
    bool success;
    std::chrono::milliseconds responseTime;
    std::string errorMessage;
};

struct NetworkHealth {
    std::string networkId;
    std::string networkName;
    std::vector<std::string> connectedContainers;
    bool isHealthy;
    std::vector<std::string> issues;
};

class NetworkDiagnostics {
public:
    NetworkDiagnostics();
    ~NetworkDiagnostics();

    // Connectivity tests
    ConnectivityTest testContainerConnectivity(const std::string& fromContainer,
                                               const std::string& toContainer);
    ConnectivityTest testInternetConnectivity(const std::string& containerId);
    ConnectivityTest testPortConnectivity(const std::string& host, int port);

    // DNS tests
    DNSTest testDNSResolution(const std::string& hostname);
    DNSTest testContainerDNS(const std::string& containerId, const std::string& hostname);
    std::vector<std::string> getDockerDNSServers();

    // Network health
    std::vector<NetworkHealth> analyzeNetworkHealth();
    NetworkHealth checkNetworkHealth(const std::string& networkId);
    bool canContainersCommunicate(const std::string& container1, const std::string& container2);

    // Network isolation
    std::vector<std::string> findIsolatedContainers();
    std::vector<std::string> findOrphanedNetworks();

    // Performance
    std::chrono::milliseconds measureLatency(const std::string& source, const std::string& destination);
    double measureBandwidth(const std::string& source, const std::string& destination);

    // Diagnostics and fixes
    std::vector<std::string> diagnoseConnectivityIssues(const std::string& containerId);
    std::vector<std::string> suggestNetworkFixes(const NetworkHealth& health);

private:
    bool pingHost(const std::string& host, int timeout = 5000);
    std::vector<std::string> resolveDNS(const std::string& hostname);
    std::string executeInContainer(const std::string& containerId, const std::string& command);
};

} // namespace network
