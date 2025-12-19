#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace diagnostics {

enum class ErrorSeverity {
    Info,
    Warning,
    Error,
    Critical
};

enum class ErrorCategory {
    Network,
    Storage,
    Permission,
    Resource,
    Configuration,
    Dependency,
    Registry,
    Unknown
};

struct DiagnosticIssue {
    std::string id;
    std::string containerId;
    std::string containerName;
    ErrorCategory category;
    ErrorSeverity severity;
    std::string title;
    std::string description;
    std::vector<std::string> affectedComponents;
    std::vector<std::string> suggestedFixes;
    bool autoFixAvailable;
    std::string detectedAt;
};

struct HealthCheck {
    std::string component;
    bool healthy;
    std::string message;
    std::vector<DiagnosticIssue> issues;
};

class ErrorDiagnostics {
public:
    ErrorDiagnostics();
    ~ErrorDiagnostics();

    // Diagnostic scanning
    std::vector<DiagnosticIssue> runFullDiagnostics();
    std::vector<DiagnosticIssue> diagnoseContainer(const std::string& containerId);
    std::vector<DiagnosticIssue> diagnoseAllContainers();

    // Specific diagnostic checks
    std::vector<DiagnosticIssue> checkPortConflicts();
    std::vector<DiagnosticIssue> checkDependencies();
    std::vector<DiagnosticIssue> checkPermissions();
    std::vector<DiagnosticIssue> checkStorage();
    std::vector<DiagnosticIssue> checkNetworkConnectivity();
    std::vector<DiagnosticIssue> checkResourceUsage();
    std::vector<DiagnosticIssue> checkConfiguration();

    // Health monitoring
    HealthCheck checkSystemHealth();
    std::vector<HealthCheck> checkAllComponentsHealth();
    bool isSystemHealthy();

    // Auto-fix capabilities
    bool canAutoFix(const DiagnosticIssue& issue);
    bool attemptAutoFix(const DiagnosticIssue& issue);
    std::vector<std::string> getManualFixSteps(const DiagnosticIssue& issue);

    // Common errors
    std::optional<DiagnosticIssue> diagnoseContainerStartFailure(const std::string& containerId);
    std::optional<DiagnosticIssue> diagnoseImagePullFailure(const std::string& image);
    std::optional<DiagnosticIssue> diagnoseNetworkError(const std::string& containerId);
    std::optional<DiagnosticIssue> diagnoseVolumeError(const std::string& containerId);

    // Issue management
    std::vector<DiagnosticIssue> getActiveIssues();
    std::vector<DiagnosticIssue> getIssuesByCategory(ErrorCategory category);
    std::vector<DiagnosticIssue> getIssuesBySeverity(ErrorSeverity severity);
    void markIssueResolved(const std::string& issueId);
    void clearResolvedIssues();

private:
    std::vector<DiagnosticIssue> activeIssues_;
    std::map<std::string, std::vector<std::string>> knownErrorPatterns_;

    // Helper methods
    void initializeErrorPatterns();
    DiagnosticIssue createIssue(ErrorCategory category, ErrorSeverity severity,
                                const std::string& title, const std::string& description);
    std::vector<std::string> generateFixSuggestions(const DiagnosticIssue& issue);
    bool matchesErrorPattern(const std::string& error, const std::string& pattern);

    // Specific error checkers
    bool checkDockerSocketPermission();
    bool checkDiskSpace();
    bool checkMemoryAvailable();
    std::vector<std::string> checkMissingDependencies(const std::string& containerId);
};

} // namespace diagnostics
