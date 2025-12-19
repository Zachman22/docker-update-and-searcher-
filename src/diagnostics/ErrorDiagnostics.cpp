#include "diagnostics/ErrorDiagnostics.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace diagnostics {

// Helper for CURL responses
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

ErrorDiagnostics::ErrorDiagnostics() {
    LOG_INFO("ErrorDiagnostics initialized");
    initializeErrorPatterns();
}

ErrorDiagnostics::~ErrorDiagnostics() = default;

void ErrorDiagnostics::initializeErrorPatterns() {
    // Port conflict patterns
    knownErrorPatterns_["port_in_use"] = {
        "bind: address already in use",
        "port is already allocated",
        "Bind for .* failed: port is already allocated"
    };

    // Permission errors
    knownErrorPatterns_["permission_denied"] = {
        "permission denied",
        "access denied",
        "Cannot connect to the Docker daemon",
        "dial unix /var/run/docker.sock: connect: permission denied"
    };

    // Image errors
    knownErrorPatterns_["image_not_found"] = {
        "Error response from daemon: pull access denied",
        "manifest unknown",
        "repository does not exist",
        "not found: manifest unknown"
    };

    // Network errors
    knownErrorPatterns_["network_error"] = {
        "network not found",
        "failed to create endpoint",
        "could not find network",
        "network.*already exists"
    };

    // Volume errors
    knownErrorPatterns_["volume_error"] = {
        "volume not found",
        "invalid volume specification",
        "error while mounting volume"
    };

    // Resource errors
    knownErrorPatterns_["out_of_resources"] = {
        "no space left on device",
        "cannot allocate memory",
        "insufficient memory",
        "disk quota exceeded"
    };
}

std::vector<DiagnosticIssue> ErrorDiagnostics::runFullDiagnostics() {
    LOG_INFO("Running full diagnostics");
    std::vector<DiagnosticIssue> issues;

    // Run all diagnostic checks
    auto portIssues = checkPortConflicts();
    auto depIssues = checkDependencies();
    auto permIssues = checkPermissions();
    auto storageIssues = checkStorage();
    auto networkIssues = checkNetworkConnectivity();
    auto resourceIssues = checkResourceUsage();
    auto configIssues = checkConfiguration();

    // Combine all issues
    issues.insert(issues.end(), portIssues.begin(), portIssues.end());
    issues.insert(issues.end(), depIssues.begin(), depIssues.end());
    issues.insert(issues.end(), permIssues.begin(), permIssues.end());
    issues.insert(issues.end(), storageIssues.begin(), storageIssues.end());
    issues.insert(issues.end(), networkIssues.begin(), networkIssues.end());
    issues.insert(issues.end(), resourceIssues.begin(), resourceIssues.end());
    issues.insert(issues.end(), configIssues.begin(), configIssues.end());

    activeIssues_ = issues;
    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::diagnoseContainer(const std::string& containerId) {
    LOG_INFO("Diagnosing container: " + containerId);

    std::vector<DiagnosticIssue> issues;

    // Get container details
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return issues;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/containers/" + containerId + "/json";
#else
    std::string url = "http://localhost/v1.41/containers/" + containerId + "/json";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        auto issue = createIssue(ErrorCategory::Unknown, ErrorSeverity::Error,
                                "Container Not Found",
                                "Failed to retrieve container information: " + containerId);
        issues.push_back(issue);
        return issues;
    }

    try {
        auto j = json::parse(response);

        // Check container state
        if (j.contains("State")) {
            auto state = j["State"];

            // Check if container failed to start
            if (state.value("Status", "") == "exited" && state.value("ExitCode", 0) != 0) {
                auto issue = createIssue(ErrorCategory::Configuration, ErrorSeverity::Error,
                                        "Container Exited with Error",
                                        "Container exited with code: " + std::to_string(state.value("ExitCode", 0)));
                issue.containerId = containerId;
                issue.containerName = j.value("Name", "");

                // Parse error message
                if (state.contains("Error") && !state["Error"].get<std::string>().empty()) {
                    issue.description += "\nError: " + state["Error"].get<std::string>();

                    // Match against known error patterns and suggest fixes
                    std::string error = state["Error"].get<std::string>();
                    for (const auto& [errorType, patterns] : knownErrorPatterns_) {
                        for (const auto& pattern : patterns) {
                            if (matchesErrorPattern(error, pattern)) {
                                issue.suggestedFixes = generateFixSuggestions(issue);
                                issue.autoFixAvailable = canAutoFix(issue);
                                break;
                            }
                        }
                    }
                }

                issues.push_back(issue);
            }

            // Check if container is restarting too much
            if (state.value("Restarting", false) || state.value("Restarting", 0) > 5) {
                auto issue = createIssue(ErrorCategory::Configuration, ErrorSeverity::Warning,
                                        "Container Restart Loop",
                                        "Container is restarting repeatedly, indicating a crash loop");
                issue.containerId = containerId;
                issue.containerName = j.value("Name", "");
                issue.suggestedFixes = {
                    "Check container logs for errors",
                    "Verify container command and entrypoint",
                    "Check resource limits",
                    "Verify dependencies are available"
                };
                issues.push_back(issue);
            }
        }

        // Check health status
        if (j.contains("State") && j["State"].contains("Health")) {
            auto health = j["State"]["Health"];
            if (health.value("Status", "") == "unhealthy") {
                auto issue = createIssue(ErrorCategory::Configuration, ErrorSeverity::Warning,
                                        "Container Unhealthy",
                                        "Container health check is failing");
                issue.containerId = containerId;
                issue.containerName = j.value("Name", "");

                if (health.contains("Log") && !health["Log"].empty()) {
                    auto lastCheck = health["Log"].back();
                    issue.description += "\nLast check output: " + lastCheck.value("Output", "");
                }

                issues.push_back(issue);
            }
        }

    } catch (const json::exception& e) {
        LOG_ERROR("JSON parsing error: " + std::string(e.what()));
    }

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::diagnoseAllContainers() {
    return runFullDiagnostics();
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkPortConflicts() {
    LOG_INFO("Checking for port conflicts");

    std::vector<DiagnosticIssue> issues;

    // Query all containers and their port bindings
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return issues;
    }

    std::string response;

#ifdef _WIN32
    std::string url = "http://localhost/v1.41/containers/json?all=true";
#else
    std::string url = "http://localhost/v1.41/containers/json?all=true";
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        try {
            auto containers = json::parse(response);
            std::map<int, std::vector<std::string>> portToContainers;

            // Map ports to containers
            for (const auto& container : containers) {
                if (container.contains("Ports")) {
                    for (const auto& port : container["Ports"]) {
                        if (port.contains("PublicPort")) {
                            int publicPort = port["PublicPort"].get<int>();
                            std::string containerName = container.value("Names", json::array())[0].get<std::string>();
                            portToContainers[publicPort].push_back(containerName);
                        }
                    }
                }
            }

            // Find conflicts
            for (const auto& [port, containers] : portToContainers) {
                if (containers.size() > 1) {
                    auto issue = createIssue(ErrorCategory::Network, ErrorSeverity::Error,
                                            "Port Conflict Detected",
                                            "Port " + std::to_string(port) + " is used by multiple containers");
                    issue.affectedComponents = containers;
                    issue.suggestedFixes = {
                        "Stop one of the containers",
                        "Change port mapping to use different host port",
                        "Use docker network instead of port binding"
                    };
                    issue.autoFixAvailable = true;
                    issues.push_back(issue);
                }
            }

        } catch (const json::exception& e) {
            LOG_ERROR("JSON parsing error: " + std::string(e.what()));
        }
    }

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkDependencies() {
    LOG_INFO("Checking dependencies");

    std::vector<DiagnosticIssue> issues;

    // Check for missing images, networks, volumes
    // This would integrate with DependencyResolver
    // For now, basic check

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkPermissions() {
    LOG_INFO("Checking permissions");

    std::vector<DiagnosticIssue> issues;

    if (!checkDockerSocketPermission()) {
        auto issue = createIssue(ErrorCategory::Permission, ErrorSeverity::Critical,
                                "Docker Socket Permission Denied",
                                "Cannot access Docker daemon socket. Permission denied.");
        issue.suggestedFixes = {
#ifdef _WIN32
            "Ensure Docker Desktop is running",
            "Check that your user is in the docker-users group",
            "Restart Docker Desktop"
#else
            "Add your user to the docker group: sudo usermod -aG docker $USER",
            "Restart your session after adding to docker group",
            "Check Docker daemon is running: sudo systemctl status docker"
#endif
        };
        issues.push_back(issue);
    }

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkStorage() {
    LOG_INFO("Checking storage");

    std::vector<DiagnosticIssue> issues;

    if (!checkDiskSpace()) {
        auto issue = createIssue(ErrorCategory::Storage, ErrorSeverity::Critical,
                                "Low Disk Space",
                                "Insufficient disk space available for Docker operations");
        issue.suggestedFixes = {
            "Remove unused images: docker image prune -a",
            "Remove unused volumes: docker volume prune",
            "Remove stopped containers: docker container prune",
            "Clean up build cache: docker builder prune"
        };
        issue.autoFixAvailable = true;
        issues.push_back(issue);
    }

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkNetworkConnectivity() {
    LOG_INFO("Checking network connectivity");

    std::vector<DiagnosticIssue> issues;

    // This would integrate with NetworkDiagnostics
    // Basic check for now

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkResourceUsage() {
    LOG_INFO("Checking resource usage");

    std::vector<DiagnosticIssue> issues;

    if (!checkMemoryAvailable()) {
        auto issue = createIssue(ErrorCategory::Resource, ErrorSeverity::Warning,
                                "Low Memory Available",
                                "System memory is running low, may affect container performance");
        issue.suggestedFixes = {
            "Stop unnecessary containers",
            "Increase Docker Desktop memory limit (Settings > Resources)",
            "Check for memory leaks in containers"
        };
        issues.push_back(issue);
    }

    return issues;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::checkConfiguration() {
    LOG_INFO("Checking configuration");

    std::vector<DiagnosticIssue> issues;

    // Check Docker daemon configuration
    // Check container configurations
    // Placeholder for now

    return issues;
}

HealthCheck ErrorDiagnostics::checkSystemHealth() {
    LOG_INFO("Checking system health");

    HealthCheck health;
    health.component = "System";
    health.healthy = true;

    auto issues = runFullDiagnostics();

    // Check for critical issues
    for (const auto& issue : issues) {
        if (issue.severity == ErrorSeverity::Critical || issue.severity == ErrorSeverity::Error) {
            health.healthy = false;
            health.message = "System has " + std::to_string(issues.size()) + " issue(s)";
            break;
        }
    }

    if (health.healthy) {
        health.message = "All systems operational";
    }

    health.issues = issues;
    return health;
}

std::vector<HealthCheck> ErrorDiagnostics::checkAllComponentsHealth() {
    LOG_INFO("Checking all components health");

    std::vector<HealthCheck> healthChecks;

    // Docker daemon health
    HealthCheck dockerHealth;
    dockerHealth.component = "Docker Daemon";
    dockerHealth.healthy = checkDockerSocketPermission();
    dockerHealth.message = dockerHealth.healthy ? "Connected" : "Connection failed";
    healthChecks.push_back(dockerHealth);

    // Storage health
    HealthCheck storageHealth;
    storageHealth.component = "Storage";
    storageHealth.healthy = checkDiskSpace();
    storageHealth.message = storageHealth.healthy ? "Sufficient space" : "Low disk space";
    healthChecks.push_back(storageHealth);

    // Memory health
    HealthCheck memoryHealth;
    memoryHealth.component = "Memory";
    memoryHealth.healthy = checkMemoryAvailable();
    memoryHealth.message = memoryHealth.healthy ? "Sufficient memory" : "Low memory";
    healthChecks.push_back(memoryHealth);

    return healthChecks;
}

bool ErrorDiagnostics::isSystemHealthy() {
    auto health = checkSystemHealth();
    return health.healthy;
}

bool ErrorDiagnostics::canAutoFix(const DiagnosticIssue& issue) {
    // Determine if issue can be automatically fixed
    switch (issue.category) {
        case ErrorCategory::Network:
            // Port conflicts can be auto-fixed by suggesting alternative ports
            return true;
        case ErrorCategory::Storage:
            // Can auto-cleanup unused resources
            return true;
        case ErrorCategory::Dependency:
            // Can auto-pull missing images
            return true;
        default:
            return false;
    }
}

bool ErrorDiagnostics::attemptAutoFix(const DiagnosticIssue& issue) {
    LOG_INFO("Attempting auto-fix for issue: " + issue.id);

    if (!canAutoFix(issue)) {
        LOG_WARNING("Issue cannot be auto-fixed: " + issue.id);
        return false;
    }

    switch (issue.category) {
        case ErrorCategory::Storage:
            // Prune unused resources
            {
                CURL* curl = curl_easy_init();
                if (!curl) return false;

                std::string response;

#ifdef _WIN32
                std::string url = "http://localhost/v1.41/images/prune";
#else
                std::string url = "http://localhost/v1.41/images/prune";
                curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
#endif

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);

                if (res == CURLE_OK) {
                    LOG_INFO("Auto-fix successful: cleaned up images");
                    return true;
                }
            }
            break;

        case ErrorCategory::Network:
            // Could suggest alternative port
            LOG_INFO("Network issue - manual intervention required");
            return false;

        default:
            return false;
    }

    return false;
}

std::vector<std::string> ErrorDiagnostics::getManualFixSteps(const DiagnosticIssue& issue) {
    return issue.suggestedFixes;
}

std::optional<DiagnosticIssue> ErrorDiagnostics::diagnoseContainerStartFailure(const std::string& containerId) {
    LOG_INFO("Diagnosing container start failure: " + containerId);

    auto issues = diagnoseContainer(containerId);
    if (!issues.empty()) {
        return issues[0];
    }

    return std::nullopt;
}

std::optional<DiagnosticIssue> ErrorDiagnostics::diagnoseImagePullFailure(const std::string& image) {
    LOG_INFO("Diagnosing image pull failure: " + image);

    auto issue = createIssue(ErrorCategory::Registry, ErrorSeverity::Error,
                            "Image Pull Failed",
                            "Failed to pull image: " + image);
    issue.suggestedFixes = {
        "Check image name and tag are correct",
        "Verify Docker Hub or registry is accessible",
        "Check if authentication is required",
        "Try pulling manually: docker pull " + image
    };

    return issue;
}

std::optional<DiagnosticIssue> ErrorDiagnostics::diagnoseNetworkError(const std::string& containerId) {
    LOG_INFO("Diagnosing network error for container: " + containerId);

    auto issue = createIssue(ErrorCategory::Network, ErrorSeverity::Error,
                            "Network Configuration Error",
                            "Container has network connectivity issues");
    issue.containerId = containerId;
    issue.suggestedFixes = {
        "Check network exists: docker network ls",
        "Recreate container with correct network",
        "Verify DNS resolution works",
        "Check firewall rules"
    };

    return issue;
}

std::optional<DiagnosticIssue> ErrorDiagnostics::diagnoseVolumeError(const std::string& containerId) {
    LOG_INFO("Diagnosing volume error for container: " + containerId);

    auto issue = createIssue(ErrorCategory::Storage, ErrorSeverity::Error,
                            "Volume Mount Error",
                            "Container has volume mounting issues");
    issue.containerId = containerId;
    issue.suggestedFixes = {
        "Check volume exists: docker volume ls",
        "Verify host path permissions (for bind mounts)",
        "Create missing volume: docker volume create <name>",
        "Check volume driver is available"
    };

    return issue;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::getActiveIssues() {
    return activeIssues_;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::getIssuesByCategory(ErrorCategory category) {
    std::vector<DiagnosticIssue> filtered;
    for (const auto& issue : activeIssues_) {
        if (issue.category == category) {
            filtered.push_back(issue);
        }
    }
    return filtered;
}

std::vector<DiagnosticIssue> ErrorDiagnostics::getIssuesBySeverity(ErrorSeverity severity) {
    std::vector<DiagnosticIssue> filtered;
    for (const auto& issue : activeIssues_) {
        if (issue.severity == severity) {
            filtered.push_back(issue);
        }
    }
    return filtered;
}

void ErrorDiagnostics::markIssueResolved(const std::string& issueId) {
    activeIssues_.erase(
        std::remove_if(activeIssues_.begin(), activeIssues_.end(),
                      [&issueId](const DiagnosticIssue& issue) {
                          return issue.id == issueId;
                      }),
        activeIssues_.end()
    );
}

void ErrorDiagnostics::clearResolvedIssues() {
    activeIssues_.clear();
}

DiagnosticIssue ErrorDiagnostics::createIssue(ErrorCategory category, ErrorSeverity severity,
                                              const std::string& title, const std::string& description) {
    DiagnosticIssue issue;

    // Generate unique ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "issue_" << timestamp << "_" << activeIssues_.size();
    issue.id = ss.str();

    issue.category = category;
    issue.severity = severity;
    issue.title = title;
    issue.description = description;
    issue.autoFixAvailable = false;

    // Format timestamp
    std::stringstream timeSs;
    timeSs << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %H:%M:%S");
    issue.detectedAt = timeSs.str();

    return issue;
}

std::vector<std::string> ErrorDiagnostics::generateFixSuggestions(const DiagnosticIssue& issue) {
    std::vector<std::string> suggestions;

    // Generate context-aware suggestions based on category
    switch (issue.category) {
        case ErrorCategory::Network:
            suggestions.push_back("Check network configuration");
            suggestions.push_back("Verify firewall rules");
            suggestions.push_back("Test connectivity with ping");
            break;
        case ErrorCategory::Storage:
            suggestions.push_back("Free up disk space");
            suggestions.push_back("Check volume permissions");
            suggestions.push_back("Verify mount points");
            break;
        case ErrorCategory::Permission:
            suggestions.push_back("Check user permissions");
            suggestions.push_back("Add user to docker group");
            suggestions.push_back("Run with appropriate privileges");
            break;
        default:
            suggestions.push_back("Check container logs for details");
            suggestions.push_back("Restart container");
            suggestions.push_back("Verify configuration");
    }

    return suggestions;
}

bool ErrorDiagnostics::matchesErrorPattern(const std::string& error, const std::string& pattern) {
    try {
        std::regex re(pattern, std::regex_constants::icase);
        return std::regex_search(error, re);
    } catch (const std::regex_error& e) {
        LOG_ERROR("Regex error: " + std::string(e.what()));
        // Fallback to simple string matching
        return error.find(pattern) != std::string::npos;
    }
}

bool ErrorDiagnostics::checkDockerSocketPermission() {
#ifdef _WIN32
    // On Windows, check if Docker Desktop is running
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string url = "http://localhost/v1.41/_ping";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
#else
    // On Linux/Mac, check socket file access
    return access("/var/run/docker.sock", R_OK | W_OK) == 0;
#endif
}

bool ErrorDiagnostics::checkDiskSpace() {
#ifdef _WIN32
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        // Require at least 1GB free
        return freeBytesAvailable.QuadPart > (1024ULL * 1024 * 1024);
    }
    return true; // Assume OK if can't check
#else
    struct statvfs stat;
    if (statvfs("/var/lib/docker", &stat) == 0) {
        unsigned long long freeSpace = stat.f_bavail * stat.f_frsize;
        // Require at least 1GB free
        return freeSpace > (1024ULL * 1024 * 1024);
    }
    return true; // Assume OK if can't check
#endif
}

bool ErrorDiagnostics::checkMemoryAvailable() {
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        // Require at least 512MB available
        return statex.ullAvailPhys > (512ULL * 1024 * 1024);
    }
    return true; // Assume OK if can't check
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        // Require at least 512MB available
        return info.freeram > (512ULL * 1024 * 1024);
    }
    return true; // Assume OK if can't check
#endif
}

std::vector<std::string> ErrorDiagnostics::checkMissingDependencies(const std::string& containerId) {
    // This would integrate with DependencyResolver
    return {};
}

} // namespace diagnostics
