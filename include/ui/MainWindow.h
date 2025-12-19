#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <memory>
#include "../docker/ContainerManager.h"
#include "../network/PortScanner.h"
#include "../update/UpdateChecker.h"
#include "../diagnostics/ErrorDiagnostics.h"
#include "../storage/Database.h"

namespace ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Container actions
    void onRefreshContainers();
    void onStartContainer();
    void onStopContainer();
    void onRestartContainer();
    void onViewLogs();

    // Update actions
    void onCheckUpdates();
    void onUpdateSelected();
    void onUpdateAll();

    // Diagnostic actions
    void onRunDiagnostics();
    void onFixIssue();
    void onViewIssueDetails();

    // Network actions
    void onScanPorts();
    void onCheckConnectivity();
    void onResolveConflict();

    // Settings
    void onOpenSettings();

    // Timer events
    void onAutoRefresh();
    void onAutoHealthCheck();

private:
    // UI initialization
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupDashboard();
    void setupContainerView();
    void setupNetworkView();
    void setupUpdatesView();
    void setupDiagnosticsView();

    // UI update methods
    void updateDashboard();
    void updateContainerTable();
    void updateIssuesList();
    void updateNetworkStatus();
    void updateStatistics();

    // Helper methods
    void showError(const std::string& message);
    void showSuccess(const std::string& message);
    void showInfo(const std::string& message);
    bool confirmAction(const std::string& message);
    QWidget* createDashboardWidget();

    // Core components
    std::shared_ptr<docker::DockerClient> dockerClient_;
    std::shared_ptr<docker::ContainerManager> containerManager_;
    std::shared_ptr<network::PortScanner> portScanner_;
    std::shared_ptr<network::NetworkDiagnostics> networkDiagnostics_;
    std::shared_ptr<update::UpdateChecker> updateChecker_;
    std::shared_ptr<diagnostics::ErrorDiagnostics> errorDiagnostics_;
    std::shared_ptr<storage::Database> database_;

    // UI components
    QTableWidget* containerTable_;
    QTableWidget* issuesTable_;
    QTableWidget* networkTable_;
    QTableWidget* updatesTable_;

    QLabel* statsRunning_;
    QLabel* statsStopped_;
    QLabel* statsIssues_;
    QLabel* statsUpdates_;

    QPushButton* btnRefresh_;
    QPushButton* btnStart_;
    QPushButton* btnStop_;
    QPushButton* btnRestart_;
    QPushButton* btnCheckUpdates_;
    QPushButton* btnUpdateAll_;
    QPushButton* btnRunDiagnostics_;

    QTimer* autoRefreshTimer_;
    QTimer* healthCheckTimer_;

    // State
    std::vector<docker::Container> containers_;
    std::vector<diagnostics::DiagnosticIssue> currentIssues_;
    std::vector<update::UpdateInfo> availableUpdates_;
};

} // namespace ui
