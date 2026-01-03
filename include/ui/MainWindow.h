#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QTabWidget>
#include <memory>
#include "../docker/ContainerManager.h"
#include "../docker/DependencyResolver.h"
#include "../network/PortScanner.h"
#include "../network/NetworkDiagnostics.h"
#include "../update/UpdateChecker.h"
#include "../diagnostics/ErrorDiagnostics.h"
#include "../storage/Database.h"

// New v0.4.0 includes
#include "../compose/ComposeStack.h"
#include "../registry/RegistryManager.h"

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
    void onRemoveContainer();
    void onViewLogs();
    void onInspectContainer();
    void onExecInContainer();

    // NEW: Container wizard
    void onCreateContainer();

    // NEW: Stack management
    void onRefreshStacks();
    void onCreateStack();
    void onDeployStack();
    void onStopStack();
    void onRemoveStack();
    void onUpdateStack();
    void onViewStackLogs();
    void onImportCompose();

    // Update actions
    void onCheckUpdates();
    void onUpdateSelected();
    void onUpdateAll();

    // Diagnostic actions
    void onRunDiagnostics();
    void onFixIssue();
    void onViewIssueDetails();
    void onExportDiagnosticReport();

    // Network actions
    void onScanPorts();
    void onCheckConnectivity();
    void onResolveConflict();

    // NEW: Registry management
    void onManageRegistries();
    void onSearchImages();

    // NEW: Theme management
    void onToggleDarkMode();
    void onChangeTheme();

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
    void setupStackView();        // NEW
    void setupNetworkView();
    void setupUpdatesView();
    void setupDiagnosticsView();

    // UI update methods
    void updateDashboard();
    void updateContainerTable();
    void updateStackTable();       // NEW
    void updateIssuesList();
    void updateNetworkStatus();
    void updateStatistics();

    // Helper methods
    void showError(const std::string& message);
    void showSuccess(const std::string& message);
    void showInfo(const std::string& message);
    bool confirmAction(const std::string& message);
    void loadThemePreference();   // NEW
    void applyTheme();             // NEW

    // Core components
    std::shared_ptr<docker::DockerClient> dockerClient_;
    std::shared_ptr<docker::ContainerManager> containerManager_;
    std::shared_ptr<docker::DependencyResolver> dependencyResolver_;
    std::shared_ptr<network::PortScanner> portScanner_;
    std::shared_ptr<network::NetworkDiagnostics> networkDiagnostics_;
    std::shared_ptr<update::UpdateChecker> updateChecker_;
    std::shared_ptr<diagnostics::ErrorDiagnostics> errorDiagnostics_;
    std::shared_ptr<storage::Database> database_;

    // NEW v0.4.0 components
    std::shared_ptr<compose::ComposeStack> composeStack_;
    std::shared_ptr<registry::RegistryManager> registryManager_;

    // UI components
    QTabWidget* tabWidget_;        // NEW: Main tab widget

    QTableWidget* containerTable_;
    QTableWidget* stackTable_;     // NEW
    QTableWidget* issuesTable_;
    QTableWidget* networkTable_;
    QTableWidget* updatesTable_;

    QLabel* statsRunning_;
    QLabel* statsStopped_;
    QLabel* statsStacks_;          // NEW
    QLabel* statsIssues_;
    QLabel* statsUpdates_;

    QPushButton* btnRefresh_;
    QPushButton* btnStart_;
    QPushButton* btnStop_;
    QPushButton* btnRestart_;
    QPushButton* btnRemove_;       // NEW
    QPushButton* btnCreateContainer_; // NEW
    QPushButton* btnViewLogs_;     // NEW

    // Stack buttons
    QPushButton* btnCreateStack_;  // NEW
    QPushButton* btnDeployStack_;  // NEW
    QPushButton* btnStopStack_;    // NEW

    QPushButton* btnCheckUpdates_;
    QPushButton* btnUpdateAll_;
    QPushButton* btnRunDiagnostics_;
    QPushButton* btnExportReport_; // NEW

    QTimer* autoRefreshTimer_;
    QTimer* healthCheckTimer_;

    // State
    std::vector<docker::Container> containers_;
    std::vector<compose::StackInfo> stacks_;  // NEW
    std::vector<diagnostics::DiagnosticIssue> currentIssues_;
    std::vector<update::UpdateInfo> availableUpdates_;

    // NEW: Theme state
    bool isDarkMode_;
};

} // namespace ui
