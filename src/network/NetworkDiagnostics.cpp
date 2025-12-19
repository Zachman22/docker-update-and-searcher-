#include "network/NetworkDiagnostics.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace network {

// Helper for CURL responses
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

NetworkDiagnostics::NetworkDiagnostics() {
    LOG_INFO("NetworkDiagnostics initialized");
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

NetworkDiagnostics::~NetworkDiagnostics() {
#ifdef _WIN32
    WSACleanup();
#endif
}

ConnectivityTest NetworkDiagnostics::testContainerConnectivity(const std::string& fromContainer,
                                                               const std::string& toContainer) {
    LOG_INFO("Testing connectivity from container " + fromContainer + " to " + toContainer);

    ConnectivityTest test;
    test.source = fromContainer;
    test.destination = toContainer;
    test.port = 0; // Not testing specific port

    auto startTime = std::chrono::high_resolution_clock::now();

    // Execute ping command inside the source container to destination container
    std::string command = "ping -c 1 -W 2 " + toContainer;
    std::string result = executeInContainer(fromContainer, command);

    auto endTime = std::chrono::high_resolution_clock::now();
    test.latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (result.find("1 received") != std::string::npos || result.find("1 packets received") != std::string::npos) {
        test.status = ConnectivityStatus::Success;
        test.errorMessage = "";
    } else if (result.find("Network is unreachable") != std::string::npos) {
        test.status = ConnectivityStatus::NetworkUnreachable;
        test.errorMessage = "Network unreachable - containers may be on different networks";
    } else if (result.find("Unknown host") != std::string::npos || result.find("Name or service not known") != std::string::npos) {
        test.status = ConnectivityStatus::DNSFailure;
        test.errorMessage = "DNS resolution failed - container name not found";
    } else {
        test.status = ConnectivityStatus::Timeout;
        test.errorMessage = "Connection timeout - firewall or network isolation";
    }

    return test;
}

ConnectivityTest NetworkDiagnostics::testInternetConnectivity(const std::string& containerId) {
    LOG_INFO("Testing internet connectivity for container: " + containerId);

    ConnectivityTest test;
    test.source = containerId;
    test.destination = "8.8.8.8"; // Google DNS
    test.port = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Try to ping Google DNS
    std::string command = "ping -c 1 -W 2 8.8.8.8";
    std::string result = executeInContainer(containerId, command);

    auto endTime = std::chrono::high_resolution_clock::now();
    test.latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (result.find("1 received") != std::string::npos || result.find("1 packets received") != std::string::npos) {
        test.status = ConnectivityStatus::Success;
        test.errorMessage = "";
    } else if (result.find("Network is unreachable") != std::string::npos) {
        test.status = ConnectivityStatus::NetworkUnreachable;
        test.errorMessage = "No internet access - check Docker network configuration";
    } else {
        test.status = ConnectivityStatus::Timeout;
        test.errorMessage = "Internet connectivity timeout";
    }

    return test;
}

ConnectivityTest NetworkDiagnostics::testPortConnectivity(const std::string& host, int port) {
    LOG_INFO("Testing port connectivity to " + host + ":" + std::to_string(port));

    ConnectivityTest test;
    test.source = "host";
    test.destination = host;
    test.port = port;

    auto startTime = std::chrono::high_resolution_clock::now();

#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        test.status = ConnectivityStatus::Failed;
        test.errorMessage = "Failed to create socket";
        test.latency = std::chrono::milliseconds(0);
        return test;
    }

    // Set timeout
    DWORD timeout = 5000; // 5 seconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (he == nullptr) {
        test.status = ConnectivityStatus::DNSFailure;
        test.errorMessage = "DNS resolution failed";
        closesocket(sock);
        test.latency = std::chrono::milliseconds(0);
        return test;
    }

    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(sock);
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        test.status = ConnectivityStatus::Failed;
        test.errorMessage = "Failed to create socket";
        test.latency = std::chrono::milliseconds(0);
        return test;
    }

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (he == nullptr) {
        test.status = ConnectivityStatus::DNSFailure;
        test.errorMessage = "DNS resolution failed";
        close(sock);
        test.latency = std::chrono::milliseconds(0);
        return test;
    }

    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
#endif

    auto endTime = std::chrono::high_resolution_clock::now();
    test.latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (result == 0) {
        test.status = ConnectivityStatus::Success;
        test.errorMessage = "";
    } else {
        test.status = ConnectivityStatus::PortClosed;
        test.errorMessage = "Port is closed or filtered";
    }

    return test;
}

DNSTest NetworkDiagnostics::testDNSResolution(const std::string& hostname) {
    LOG_INFO("Testing DNS resolution for: " + hostname);

    DNSTest test;
    test.hostname = hostname;

    auto startTime = std::chrono::high_resolution_clock::now();

    test.resolvedIPs = resolveDNS(hostname);

    auto endTime = std::chrono::high_resolution_clock::now();
    test.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (!test.resolvedIPs.empty()) {
        test.success = true;
        test.errorMessage = "";
    } else {
        test.success = false;
        test.errorMessage = "DNS resolution failed - hostname not found";
    }

    return test;
}

DNSTest NetworkDiagnostics::testContainerDNS(const std::string& containerId, const std::string& hostname) {
    LOG_INFO("Testing DNS from container " + containerId + " for hostname: " + hostname);

    DNSTest test;
    test.hostname = hostname;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Use nslookup or getent in container
    std::string command = "nslookup " + hostname + " || getent hosts " + hostname;
    std::string result = executeInContainer(containerId, command);

    auto endTime = std::chrono::high_resolution_clock::now();
    test.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (result.find("Address:") != std::string::npos || result.find("NXDOMAIN") == std::string::npos) {
        test.success = true;
        test.errorMessage = "";

        // Parse IPs from result (basic parsing)
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Address:") != std::string::npos && line.find("#53") == std::string::npos) {
                size_t pos = line.find("Address:") + 9;
                std::string ip = line.substr(pos);
                // Trim whitespace
                ip.erase(0, ip.find_first_not_of(" \t\r\n"));
                ip.erase(ip.find_last_not_of(" \t\r\n") + 1);
                if (!ip.empty()) {
                    test.resolvedIPs.push_back(ip);
                }
            }
        }
    } else {
        test.success = false;
        test.errorMessage = "DNS resolution failed from container";
    }

    return test;
}

std::vector<std::string> NetworkDiagnostics::getDockerDNSServers() {
    LOG_INFO("Retrieving Docker DNS servers");

    std::vector<std::string> dnsServers;

    // Query Docker daemon for DNS configuration
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return dnsServers;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/info";
#else
    std::string url = "http://localhost/v1.41/info";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        // Parse JSON response to extract DNS servers
        // For now, return default Docker DNS
        dnsServers.push_back("127.0.0.11"); // Docker's embedded DNS
    }

    return dnsServers;
}

std::vector<NetworkHealth> NetworkDiagnostics::analyzeNetworkHealth() {
    LOG_INFO("Analyzing network health for all Docker networks");

    std::vector<NetworkHealth> healthReports;

    // Query Docker for all networks
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return healthReports;
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

    if (res == CURLE_OK) {
        // Parse network list and check each one
        // Simplified: just report that networks exist
        NetworkHealth health;
        health.networkId = "bridge";
        health.networkName = "bridge";
        health.isHealthy = true;
        healthReports.push_back(health);
    }

    return healthReports;
}

NetworkHealth NetworkDiagnostics::checkNetworkHealth(const std::string& networkId) {
    LOG_INFO("Checking health of network: " + networkId);

    NetworkHealth health;
    health.networkId = networkId;
    health.isHealthy = true;

    // Query Docker for network details
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        health.isHealthy = false;
        health.issues.push_back("Failed to connect to Docker daemon");
        return health;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/networks/" + networkId;
#else
    std::string url = "http://localhost/v1.41/networks/" + networkId;
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        health.isHealthy = false;
        health.issues.push_back("Network not found or inaccessible");
    }

    return health;
}

bool NetworkDiagnostics::canContainersCommunicate(const std::string& container1,
                                                  const std::string& container2) {
    LOG_INFO("Checking if containers can communicate: " + container1 + " -> " + container2);

    ConnectivityTest test = testContainerConnectivity(container1, container2);
    return test.status == ConnectivityStatus::Success;
}

std::vector<std::string> NetworkDiagnostics::findIsolatedContainers() {
    LOG_INFO("Finding isolated containers");

    std::vector<std::string> isolatedContainers;

    // Query all containers and check their network connections
    // A container is isolated if it's not connected to any custom networks
    // and cannot reach the internet

    // This would require integration with DockerClient to list containers
    // For now, return empty list as placeholder

    return isolatedContainers;
}

std::vector<std::string> NetworkDiagnostics::findOrphanedNetworks() {
    LOG_INFO("Finding orphaned networks");

    std::vector<std::string> orphanedNetworks;

    // Query all networks and find those with no connected containers
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return orphanedNetworks;
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

    if (res == CURLE_OK) {
        // Parse response and find networks with no containers
        // Placeholder implementation
    }

    return orphanedNetworks;
}

std::chrono::milliseconds NetworkDiagnostics::measureLatency(const std::string& source,
                                                             const std::string& destination) {
    LOG_INFO("Measuring latency from " + source + " to " + destination);

    if (source == "host") {
        // Measure from host
        return testPortConnectivity(destination, 80).latency;
    } else {
        // Measure from container
        return testContainerConnectivity(source, destination).latency;
    }
}

double NetworkDiagnostics::measureBandwidth(const std::string& source,
                                           const std::string& destination) {
    LOG_INFO("Measuring bandwidth from " + source + " to " + destination);

    // Bandwidth measurement would require transferring data
    // Using iperf or similar tool inside containers
    // Placeholder: return 0 for now
    return 0.0;
}

std::vector<std::string> NetworkDiagnostics::diagnoseConnectivityIssues(const std::string& containerId) {
    LOG_INFO("Diagnosing connectivity issues for container: " + containerId);

    std::vector<std::string> issues;

    // Test internet connectivity
    ConnectivityTest internetTest = testInternetConnectivity(containerId);
    if (internetTest.status != ConnectivityStatus::Success) {
        issues.push_back("No internet connectivity: " + internetTest.errorMessage);
    }

    // Test DNS
    DNSTest dnsTest = testContainerDNS(containerId, "google.com");
    if (!dnsTest.success) {
        issues.push_back("DNS resolution failing: " + dnsTest.errorMessage);
    }

    // Check if container is on a network
    // This would require integration with DockerClient

    if (issues.empty()) {
        issues.push_back("No connectivity issues detected");
    }

    return issues;
}

std::vector<std::string> NetworkDiagnostics::suggestNetworkFixes(const NetworkHealth& health) {
    LOG_INFO("Suggesting fixes for network: " + health.networkName);

    std::vector<std::string> suggestions;

    if (!health.isHealthy) {
        for (const auto& issue : health.issues) {
            if (issue.find("not found") != std::string::npos) {
                suggestions.push_back("Recreate the network with: docker network create " + health.networkName);
            } else if (issue.find("no containers") != std::string::npos) {
                suggestions.push_back("Remove unused network with: docker network rm " + health.networkId);
            } else {
                suggestions.push_back("Check Docker daemon logs for network errors");
            }
        }
    }

    if (suggestions.empty()) {
        suggestions.push_back("Network appears healthy - no fixes needed");
    }

    return suggestions;
}

bool NetworkDiagnostics::pingHost(const std::string& host, int timeout) {
    LOG_INFO("Pinging host: " + host);

#ifdef _WIN32
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    unsigned long ipaddr = inet_addr(host.c_str());
    if (ipaddr == INADDR_NONE) {
        struct hostent* he = gethostbyname(host.c_str());
        if (he == nullptr) {
            IcmpCloseHandle(hIcmpFile);
            return false;
        }
        ipaddr = *(unsigned long*)he->h_addr_list[0];
    }

    char sendData[32] = "Data for ping test";
    char replyBuffer[sizeof(ICMP_ECHO_REPLY) + 32];

    DWORD dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, sendData, sizeof(sendData),
                                   NULL, replyBuffer, sizeof(replyBuffer), timeout);

    IcmpCloseHandle(hIcmpFile);
    return dwRetVal != 0;
#else
    // On Linux, use system ping command
    std::string command = "ping -c 1 -W " + std::to_string(timeout / 1000) + " " + host + " >/dev/null 2>&1";
    int result = system(command.c_str());
    return WIFEXITED(result) && WEXITSTATUS(result) == 0;
#endif
}

std::vector<std::string> NetworkDiagnostics::resolveDNS(const std::string& hostname) {
    LOG_INFO("Resolving DNS for: " + hostname);

    std::vector<std::string> ips;

    struct hostent* he = gethostbyname(hostname.c_str());
    if (he == nullptr) {
        return ips;
    }

    for (int i = 0; he->h_addr_list[i] != nullptr; i++) {
        struct in_addr addr;
        memcpy(&addr, he->h_addr_list[i], sizeof(struct in_addr));
        ips.push_back(inet_ntoa(addr));
    }

    return ips;
}

std::string NetworkDiagnostics::executeInContainer(const std::string& containerId,
                                                   const std::string& command) {
    LOG_INFO("Executing command in container " + containerId + ": " + command);

    // Create exec instance via Docker API
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return "";
    }

    std::string response;
    std::string execData = "{\"AttachStdout\":true,\"AttachStderr\":true,\"Cmd\":[\"sh\",\"-c\",\"" + command + "\"]}";

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/containers/" + containerId + "/exec";
#else
    std::string url = "http://localhost/v1.41/containers/" + containerId + "/exec";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, execData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to create exec instance");
        return "";
    }

    // Parse exec ID and start exec
    // Simplified: just return response for now
    return response;
}

} // namespace network
