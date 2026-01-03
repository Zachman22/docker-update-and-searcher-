#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <memory>

namespace docker {
    class DockerClient;
}

namespace ui {

/**
 * LogsViewerDialog - Dialog for viewing container logs
 */
class LogsViewerDialog : public QDialog {
    Q_OBJECT

public:
    explicit LogsViewerDialog(std::shared_ptr<docker::DockerClient> dockerClient,
                             const std::string& containerId,
                             const std::string& containerName,
                             QWidget* parent = nullptr);
    ~LogsViewerDialog();

private slots:
    void refreshLogs();
    void toggleAutoRefresh(bool enabled);
    void toggleFollowMode(bool enabled);
    void clearLogs();
    void saveLogsToFile();
    void updateRefreshInterval(int seconds);
    void copyLogsToClipboard();
    void searchLogs();

private:
    void setupUI();
    void loadInitialLogs();
    void applyFilters();

    std::shared_ptr<docker::DockerClient> dockerClient_;
    std::string containerId_;
    std::string containerName_;

    // UI Components
    QTextEdit* logsTextEdit_;
    QPushButton* refreshButton_;
    QPushButton* clearButton_;
    QPushButton* saveButton_;
    QPushButton* copyButton_;
    QPushButton* searchButton_;
    QCheckBox* autoRefreshCheckbox_;
    QCheckBox* followCheckbox_;
    QCheckBox* timestampsCheckbox_;
    QCheckBox* errorOnlyCheckbox_;
    QSpinBox* tailLinesSpinBox_;
    QSpinBox* refreshIntervalSpinBox_;

    // State
    QTimer* refreshTimer_;
    bool followMode_;
    int tailLines_;
    std::string lastLogContent_;
};

} // namespace ui
