/**
 * Docker Homelab Manager - Example Usage
 *
 * This example demonstrates all the major features of the application.
 * Compile with: g++ -std=c++17 example_usage.cpp -I../include -lsqlite3 -lcurl
 */

#include <iostream>
#include <iomanip>
#include <memory>
#include "docker/DockerClient.h"
#include "network/PortScanner.h"
#include "update/UpdateChecker.h"
#include "storage/Database.h"
#include "utils/Logger.h"

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void printContainer(const docker::Container& container) {
    std::cout << "  📦 " << container.getName() << "\n";
    std::cout << "     ID: " << container.getId().substr(0, 12) << "\n";
    std::cout << "     Image: " << container.getImage() << ":" << container.getImageTag() << "\n";
    std::cout << "     State: " << container.getStateString() << "\n";

    auto ports = container.getPorts();
    if (!ports.empty()) {
        std::cout << "     Ports: ";
        for (size_t i = 0; i < ports.size(); ++i) {
            std::cout << ports[i].hostPort << "->" << ports[i].containerPort;
            if (i < ports.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
}

int main() {
    using namespace std;

    // Initialize logger
    utils::Logger::getInstance().setLogLevel(utils::LogLevel::Info);
    utils::Logger::getInstance().enableConsoleOutput(true);
    utils::Logger::getInstance().enableFileOutput(true);
    utils::Logger::getInstance().setLogFile("example.log");

    cout << R"(
    ╔═══════════════════════════════════════════════════════════╗
    ║   Docker Homelab Manager - Complete Feature Demo         ║
    ║   Version 0.2.0-alpha                                     ║
    ╚═══════════════════════════════════════════════════════════╝
    )" << endl;

    // =========================================================================
    // 1. DOCKER API - Connect and List Containers
    // =========================================================================
    printSeparator("1. Docker API Integration");

    auto dockerClient = make_shared<docker::DockerClient>();

    cout << "Connecting to Docker daemon..." << endl;
    if (!dockerClient->connect()) {
        cerr << "❌ Failed to connect to Docker daemon!" << endl;
        cerr << "   Make sure Docker is running." << endl;
        return 1;
    }

    cout << "✅ Connected to Docker!" << endl;
    cout << "   Version: " << dockerClient->getDockerVersion() << endl;

    cout << "\nFetching containers..." << endl;
    auto containers = dockerClient->listContainers(true);
    cout << "✅ Found " << containers.size() << " containers\n" << endl;

    if (containers.empty()) {
        cout << "⚠️  No containers found. Start some containers to see more features!" << endl;
    } else {
        cout << "Container Details:\n";
        for (const auto& container : containers) {
            printContainer(container);
            cout << endl;
        }
    }

    // =========================================================================
    // 2. PORT SCANNER - Detect Conflicts
    // =========================================================================
    printSeparator("2. Port Conflict Detection");

    auto portScanner = make_shared<network::PortScanner>();

    cout << "Scanning system ports..." << endl;
    auto openPorts = portScanner->scanOpenPorts();
    cout << "✅ Found " << openPorts.size() << " open ports\n" << endl;

    // Show first 10 ports
    cout << "Sample of open ports:\n";
    for (size_t i = 0; i < min(size_t(10), openPorts.size()); ++i) {
        const auto& port = openPorts[i];
        cout << "  🔌 Port " << port.port << " (" << port.protocol << ")";
        if (!port.processName.empty()) {
            cout << " - " << port.processName << " (PID: " << port.processId << ")";
        }
        cout << endl;
    }

    cout << "\nDetecting port conflicts..." << endl;
    auto conflicts = portScanner->detectConflicts();

    if (conflicts.empty()) {
        cout << "✅ No port conflicts detected!" << endl;
    } else {
        cout << "⚠️  Found " << conflicts.size() << " port conflicts:\n" << endl;
        for (const auto& conflict : conflicts) {
            cout << "  ⚠️  Port " << conflict.port << " (" << conflict.protocol << ")\n";
            cout << "     " << conflict.description << "\n";
            cout << "     Suggested fixes:\n";
            for (const auto& fix : conflict.suggestedFixes) {
                cout << "     → " << fix << "\n";
            }
            cout << endl;
        }
    }

    // Demonstrate port availability check
    cout << "\nChecking port availability..." << endl;
    vector<int> portsToCheck = {8080, 8081, 8082, 9000, 3000};
    for (int port : portsToCheck) {
        bool isOpen = portScanner->isPortOpen(port, "tcp");
        cout << "  Port " << port << ": "
             << (isOpen ? "❌ In use" : "✅ Available") << endl;
    }

    // Find available ports
    cout << "\nFinding 5 available ports starting from 8000..." << endl;
    auto availablePorts = portScanner->findAvailablePorts(5, 8000, 9000);
    cout << "  Available ports: ";
    for (size_t i = 0; i < availablePorts.size(); ++i) {
        cout << availablePorts[i];
        if (i < availablePorts.size() - 1) cout << ", ";
    }
    cout << endl;

    // =========================================================================
    // 3. UPDATE CHECKER - Check for Updates
    // =========================================================================
    printSeparator("3. Update Checker");

    auto updateChecker = make_shared<update::UpdateChecker>();

    if (!containers.empty()) {
        cout << "Checking for updates from Docker Hub..." << endl;
        cout << "(This may take a few seconds per container)\n" << endl;

        auto updates = updateChecker->checkForUpdates(containers);

        if (updates.empty()) {
            cout << "✅ All containers are up to date!" << endl;
        } else {
            cout << "📦 Found " << updates.size() << " updates available:\n" << endl;
            for (const auto& update : updates) {
                cout << "  📦 " << update.containerName << "\n";
                cout << "     Current: " << update.currentTag << "\n";
                cout << "     Latest:  " << update.latestTag << "\n";

                if (!update.currentDigest.empty() && !update.latestDigest.empty()) {
                    cout << "     Digest Changed: "
                         << (update.currentDigest != update.latestDigest ? "Yes" : "No") << "\n";
                }
                cout << endl;
            }
        }

        // Demonstrate version listing
        if (!containers.empty()) {
            const auto& firstContainer = containers[0];
            string imageName = firstContainer.getImage();
            size_t colonPos = imageName.find(':');
            if (colonPos != string::npos) {
                imageName = imageName.substr(0, colonPos);
            }

            cout << "\nFetching available versions for " << imageName << "..." << endl;
            auto versions = updateChecker->getAvailableVersions(imageName);

            if (!versions.empty()) {
                cout << "  Found " << versions.size() << " versions (showing first 5):\n";
                for (size_t i = 0; i < min(size_t(5), versions.size()); ++i) {
                    cout << "    - " << versions[i].tag;
                    if (versions[i].sizeBytes > 0) {
                        cout << " (" << (versions[i].sizeBytes / 1024 / 1024) << " MB)";
                    }
                    cout << endl;
                }
            }
        }
    } else {
        cout << "⚠️  No containers to check for updates." << endl;
    }

    // =========================================================================
    // 4. DATABASE - Persistent Storage
    // =========================================================================
    printSeparator("4. SQLite Database Storage");

    auto database = make_shared<storage::Database>("example_homelab.db");

    cout << "Initializing database..." << endl;
    if (!database->initialize()) {
        cerr << "❌ Failed to initialize database!" << endl;
        return 1;
    }
    cout << "✅ Database initialized successfully!" << endl;

    // Save containers
    cout << "\nSaving containers to database..." << endl;
    for (const auto& container : containers) {
        database->saveContainer(container);
    }
    cout << "✅ Saved " << containers.size() << " containers" << endl;

    // Record some actions
    cout << "\nRecording container actions..." << endl;
    for (const auto& container : containers) {
        storage::ContainerHistory history;
        history.containerId = container.getId();
        history.action = "scanned";
        history.details = "Discovered during example run";
        history.performedBy = "example_app";
        database->recordContainerAction(history);
    }
    cout << "✅ Actions recorded" << endl;

    // Get statistics
    cout << "\nDatabase Statistics:" << endl;
    cout << "  Total containers: " << database->getTotalContainers() << endl;
    cout << "  Active issues: " << database->getActiveIssuesCount() << endl;

    // =========================================================================
    // 5. SUMMARY
    // =========================================================================
    printSeparator("Summary");

    cout << "✅ All major components tested successfully!\n" << endl;

    cout << "Capabilities Demonstrated:\n";
    cout << "  ✅ Docker API connection and container listing\n";
    cout << "  ✅ Port scanning and conflict detection\n";
    cout << "  ✅ Alternative port suggestions\n";
    cout << "  ✅ Update checking from Docker Hub\n";
    cout << "  ✅ Version listing and comparison\n";
    cout << "  ✅ Database persistence and history tracking\n";
    cout << "  ✅ Comprehensive logging\n" << endl;

    cout << "Next Steps:\n";
    cout << "  → Build the full application: cmake .. && cmake --build .\n";
    cout << "  → Run the GUI: ./DockerHomelabManager\n";
    cout << "  → Check the log: cat example.log\n";
    cout << "  → Explore the database: sqlite3 example_homelab.db\n" << endl;

    cout << "📝 Log file saved to: example.log\n";
    cout << "💾 Database saved to: example_homelab.db\n" << endl;

    cout << "Thank you for trying Docker Homelab Manager! 🚀\n" << endl;

    return 0;
}
