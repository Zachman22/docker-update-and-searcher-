#pragma once

#include <string>
#include <vector>
#include <memory>

namespace diagnostics {
    struct DiagnosticIssue;
}

namespace docker {
    class DockerClient;
    struct Container;
}

namespace diagnostics {

/**
 * DiagnosticReporter - Export diagnostic reports in various formats
 */
class DiagnosticReporter {
public:
    explicit DiagnosticReporter(std::shared_ptr<docker::DockerClient> dockerClient);
    ~DiagnosticReporter();

    enum class ReportFormat {
        JSON,
        HTML,
        Markdown,
        PDF,
        Text
    };

    struct SystemReport {
        std::string timestamp;
        std::string dockerVersion;
        std::string systemInfo;
        int totalContainers = 0;
        int runningContainers = 0;
        int totalImages = 0;
        int totalVolumes = 0;
        int totalNetworks = 0;
        std::vector<DiagnosticIssue> issues;
        std::vector<docker::Container> containers;
    };

    /**
     * Generate full system diagnostic report
     * @return System report
     */
    SystemReport generateSystemReport();

    /**
     * Export report to file
     * @param report Report to export
     * @param filePath Output file path
     * @param format Report format
     * @return Success status
     */
    bool exportReport(const SystemReport& report, const std::string& filePath, ReportFormat format);

    /**
     * Export to JSON format
     */
    std::string toJSON(const SystemReport& report);

    /**
     * Export to HTML format
     */
    std::string toHTML(const SystemReport& report);

    /**
     * Export to Markdown format
     */
    std::string toMarkdown(const SystemReport& report);

    /**
     * Export to plain text format
     */
    std::string toText(const SystemReport& report);

    /**
     * Get last error
     */
    std::string getLastError() const;

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    std::string lastError_;

    std::string generateHTMLHeader();
    std::string generateHTMLFooter();
    std::string issueToHTML(const DiagnosticIssue& issue);
    std::string containerToHTML(const docker::Container& container);
};

} // namespace diagnostics
