#include "network/PortScanner.h"
#include "utils/Logger.h"
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#endif

namespace network {

PortScanner::PortScanner() {
    LOG_INFO("PortScanner initialized");
    #ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif
}

PortScanner::~PortScanner() {
    #ifdef _WIN32
    WSACleanup();
    #endif
}

std::vector<PortInfo> PortScanner::scanOpenPorts() {
    LOG_INFO("Scanning all open ports");
    #ifdef _WIN32
        return scanOpenPortsWindows();
    #elif __linux__
        return scanOpenPortsLinux();
    #elif __APPLE__
        return scanOpenPortsMacOS();
    #else
        LOG_ERROR("Unsupported platform for port scanning");
        return {};
    #endif
}

std::vector<PortInfo> PortScanner::scanPortRange(int startPort, int endPort) {
    std::vector<PortInfo> openPorts;
    LOG_INFO("Scanning port range: " + std::to_string(startPort) + "-" + std::to_string(endPort));

    for (int port = startPort; port <= endPort; ++port) {
        if (isPortOpen(port, "tcp")) {
            auto portInfo = getPortInfo(port, "tcp");
            if (portInfo) {
                openPorts.push_back(*portInfo);
            }
        }
    }

    return openPorts;
}

bool PortScanner::isPortOpen(int port, const std::string& protocol) {
    #ifdef _WIN32
    SOCKET sock = socket(AF_INET, protocol == "tcp" ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int result = bind(sock, (sockaddr*)&addr, sizeof(addr));
    closesocket(sock);

    // If bind fails, port is in use
    return result == SOCKET_ERROR;
    #else
    int sock = socket(AF_INET, protocol == "tcp" ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int result = bind(sock, (sockaddr*)&addr, sizeof(addr));
    close(sock);

    return result < 0;  // If bind fails, port is in use
    #endif
}

std::optional<PortInfo> PortScanner::getPortInfo(int port, const std::string& protocol) {
    auto allPorts = scanOpenPorts();
    for (const auto& portInfo : allPorts) {
        if (portInfo.port == port && portInfo.protocol == protocol) {
            return portInfo;
        }
    }
    return std::nullopt;
}

std::vector<PortConflict> PortScanner::detectConflicts() {
    std::vector<PortConflict> conflicts;
    std::map<int, std::vector<PortInfo>> portMap;

    // Group ports by port number
    auto ports = scanOpenPorts();
    for (const auto& portInfo : ports) {
        portMap[portInfo.port].push_back(portInfo);
    }

    // Find conflicts (multiple processes on same port)
    for (const auto& [port, processes] : portMap) {
        if (processes.size() > 1) {
            PortConflict conflict;
            conflict.port = port;
            conflict.protocol = processes[0].protocol;
            conflict.conflictingProcesses = processes;
            conflict.description = "Port " + std::to_string(port) + " is used by " +
                                 std::to_string(processes.size()) + " processes";
            conflict.suggestedFixes = generateFixSuggestions(conflict);
            conflicts.push_back(conflict);
        }
    }

    LOG_INFO("Found " + std::to_string(conflicts.size()) + " port conflicts");
    return conflicts;
}

std::vector<PortConflict> PortScanner::detectConflictsForContainer(const std::string& containerId) {
    std::vector<PortConflict> conflicts;
    // TODO: Get container port mappings and check for conflicts
    return conflicts;
}

bool PortScanner::wouldCauseConflict(int port, const std::string& protocol) {
    return isPortOpen(port, protocol);
}

std::vector<int> PortScanner::findAvailablePorts(int count, int startPort, int endPort) {
    std::vector<int> availablePorts;

    for (int port = startPort; port <= endPort && availablePorts.size() < count; ++port) {
        if (!isPortOpen(port, "tcp")) {
            availablePorts.push_back(port);
        }
    }

    return availablePorts;
}

int PortScanner::findNextAvailablePort(int startPort) {
    auto available = findAvailablePorts(1, startPort, startPort + 1000);
    return available.empty() ? -1 : available[0];
}

std::map<std::string, std::vector<PortInfo>> PortScanner::getPortsByContainer() {
    std::map<std::string, std::vector<PortInfo>> portsByContainer;

    auto ports = scanOpenPorts();
    for (const auto& portInfo : ports) {
        if (!portInfo.containerId.empty()) {
            portsByContainer[portInfo.containerId].push_back(portInfo);
        }
    }

    return portsByContainer;
}

int PortScanner::suggestAlternativePort(int conflictingPort) {
    // Try next 100 ports
    int suggested = findNextAvailablePort(conflictingPort + 1);
    if (suggested != -1) {
        LOG_INFO("Suggesting alternative port " + std::to_string(suggested) +
                 " instead of " + std::to_string(conflictingPort));
    }
    return suggested;
}

std::vector<std::string> PortScanner::generateFixSuggestions(const PortConflict& conflict) {
    std::vector<std::string> suggestions;

    suggestions.push_back("Stop one of the conflicting processes");

    int altPort = suggestAlternativePort(conflict.port);
    if (altPort != -1) {
        suggestions.push_back("Use alternative port: " + std::to_string(altPort));
    }

    for (const auto& process : conflict.conflictingProcesses) {
        if (!process.processName.empty()) {
            suggestions.push_back("Stop process: " + process.processName +
                                " (PID: " + std::to_string(process.processId) + ")");
        }
    }

    return suggestions;
}

#ifdef _WIN32
std::vector<PortInfo> PortScanner::scanOpenPortsWindows() {
    std::vector<PortInfo> ports;

    // Get TCP table
    PMIB_TCPTABLE_OWNER_PID pTcpTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // First call to get size
    pTcpTable = (MIB_TCPTABLE_OWNER_PID*)malloc(sizeof(MIB_TCPTABLE_OWNER_PID));
    if (pTcpTable == nullptr) {
        return ports;
    }

    dwSize = sizeof(MIB_TCPTABLE_OWNER_PID);
    dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET,
                                   TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        free(pTcpTable);
        pTcpTable = (MIB_TCPTABLE_OWNER_PID*)malloc(dwSize);
        if (pTcpTable == nullptr) {
            return ports;
        }
    }

    // Second call to get actual data
    dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET,
                                   TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
            MIB_TCPROW_OWNER_PID row = pTcpTable->table[i];

            // Only include LISTENING ports
            if (row.dwState == MIB_TCP_STATE_LISTEN) {
                PortInfo info;
                info.port = ntohs((u_short)row.dwLocalPort);
                info.protocol = "tcp";
                info.isOpen = true;
                info.processId = row.dwOwningPid;

                // Get process name
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                             FALSE, row.dwOwningPid);
                if (hProcess) {
                    char processName[MAX_PATH];
                    if (GetModuleBaseNameA(hProcess, NULL, processName, MAX_PATH)) {
                        info.processName = processName;
                    }
                    CloseHandle(hProcess);
                }

                // Try to determine if it's a Docker container
                // This is a heuristic - check if process name contains "docker" or "containerd"
                if (info.processName.find("docker") != std::string::npos ||
                    info.processName.find("containerd") != std::string::npos) {
                    // Would need Docker API to get actual container ID
                    info.containerName = "docker-container";
                }

                ports.push_back(info);
            }
        }
    }

    free(pTcpTable);

    // Get UDP table
    PMIB_UDPTABLE_OWNER_PID pUdpTable;
    dwSize = 0;

    pUdpTable = (MIB_UDPTABLE_OWNER_PID*)malloc(sizeof(MIB_UDPTABLE_OWNER_PID));
    if (pUdpTable == nullptr) {
        return ports;
    }

    dwSize = sizeof(MIB_UDPTABLE_OWNER_PID);
    dwRetVal = GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET,
                                   UDP_TABLE_OWNER_PID, 0);

    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        free(pUdpTable);
        pUdpTable = (MIB_UDPTABLE_OWNER_PID*)malloc(dwSize);
        if (pUdpTable == nullptr) {
            return ports;
        }
    }

    dwRetVal = GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET,
                                   UDP_TABLE_OWNER_PID, 0);

    if (dwRetVal == NO_ERROR) {
        for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
            MIB_UDPROW_OWNER_PID row = pUdpTable->table[i];

            PortInfo info;
            info.port = ntohs((u_short)row.dwLocalPort);
            info.protocol = "udp";
            info.isOpen = true;
            info.processId = row.dwOwningPid;

            // Get process name
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                         FALSE, row.dwOwningPid);
            if (hProcess) {
                char processName[MAX_PATH];
                if (GetModuleBaseNameA(hProcess, NULL, processName, MAX_PATH)) {
                    info.processName = processName;
                }
                CloseHandle(hProcess);
            }

            ports.push_back(info);
        }
    }

    free(pUdpTable);

    LOG_INFO("Scanned " + std::to_string(ports.size()) + " open ports on Windows");
    return ports;
}
#endif

#ifdef __linux__
std::vector<PortInfo> PortScanner::scanOpenPortsLinux() {
    std::vector<PortInfo> ports;

    // Parse /proc/net/tcp
    std::ifstream tcpFile("/proc/net/tcp");
    if (tcpFile.is_open()) {
        std::string line;
        std::getline(tcpFile, line); // Skip header

        while (std::getline(tcpFile, line)) {
            std::istringstream iss(line);
            int slot;
            std::string localAddr, remAddr, st, txQueue, rxQueue, tr, tmWhen, retrnsmt;
            int uid, timeout, inode;

            iss >> slot >> localAddr >> remAddr >> st >> txQueue >> rxQueue >>
                tr >> tmWhen >> retrnsmt >> uid >> timeout >> inode;

            // Extract port from local address (format: "0100007F:1F90" = 127.0.0.1:8080)
            size_t colonPos = localAddr.find(':');
            if (colonPos != std::string::npos) {
                std::string portHex = localAddr.substr(colonPos + 1);
                int port = std::stoi(portHex, nullptr, 16);

                // Check if listening (state 0A = LISTEN)
                if (st == "0A") {
                    PortInfo info;
                    info.port = port;
                    info.protocol = "tcp";
                    info.isOpen = true;
                    info.processId = 0; // Would need to parse /proc/<pid>/fd to find process

                    ports.push_back(info);
                }
            }
        }
        tcpFile.close();
    }

    // Parse /proc/net/udp
    std::ifstream udpFile("/proc/net/udp");
    if (udpFile.is_open()) {
        std::string line;
        std::getline(udpFile, line); // Skip header

        while (std::getline(udpFile, line)) {
            std::istringstream iss(line);
            int slot;
            std::string localAddr, remAddr, st, txQueue, rxQueue, tr, tmWhen, retrnsmt;
            int uid, timeout, inode;

            iss >> slot >> localAddr >> remAddr >> st >> txQueue >> rxQueue >>
                tr >> tmWhen >> retrnsmt >> uid >> timeout >> inode;

            size_t colonPos = localAddr.find(':');
            if (colonPos != std::string::npos) {
                std::string portHex = localAddr.substr(colonPos + 1);
                int port = std::stoi(portHex, nullptr, 16);

                PortInfo info;
                info.port = port;
                info.protocol = "udp";
                info.isOpen = true;
                info.processId = 0;

                ports.push_back(info);
            }
        }
        udpFile.close();
    }

    LOG_INFO("Scanned " + std::to_string(ports.size()) + " open ports on Linux");
    return ports;
}
#endif

#ifdef __APPLE__
std::vector<PortInfo> PortScanner::scanOpenPortsMacOS() {
    std::vector<PortInfo> ports;
    // TODO: Implement using lsof or netstat
    // For now, return empty
    LOG_WARNING("macOS port scanning not yet implemented");
    return ports;
}
#endif

PortInfo PortScanner::getProcessInfo(int port, const std::string& protocol) {
    PortInfo info;
    info.port = port;
    info.protocol = protocol;
    info.isOpen = false;

    // Implementation depends on platform
    return info;
}

std::string PortScanner::getContainerIdFromProcess(int processId) {
    // TODO: Map process ID to Docker container ID
    // This would require parsing Docker container details or using cgroups on Linux
    return "";
}

} // namespace network
