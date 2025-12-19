#include "ui/MainWindow.h"
#include "utils/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QTabWidget>
#include <QGroupBox>

namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , autoRefreshTimer_(new QTimer(this))
    , healthCheckTimer_(new QTimer(this))
{
    LOG_INFO("Initializing MainWindow");

    // Initialize core components
    dockerClient_ = std::make_shared<docker::DockerClient>();
    database_ = std::make_shared<storage::Database>("homelab_manager.db");

    // Initialize database
    if (!database_->initialize()) {
        LOG_ERROR("Failed to initialize database");
    }

    // Initialize managers
    containerManager_ = std::make_shared<docker::ContainerManager>(dockerClient_);
    portScanner_ = std::make_shared<network::PortScanner>();
    networkDiagnostics_ = std::make_shared<network::NetworkDiagnostics>();
    updateChecker_ = std::make_shared<update::UpdateChecker>();
    errorDiagnostics_ = std::make_shared<diagnostics::ErrorDiagnostics>();

    // Setup UI
    setupUI();
    setupMenuBar();
    setupToolBar();

    // Connect timers
    connect(autoRefreshTimer_, &QTimer::timeout, this, &MainWindow::onAutoRefresh);
    connect(healthCheckTimer_, &QTimer::timeout, this, &MainWindow::onAutoHealthCheck);

    // Start timers
    autoRefreshTimer_->start(30000); // 30 seconds
    healthCheckTimer_->start(60000); // 60 seconds

    // Initial refresh
    onRefreshContainers();

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    LOG_INFO("Shutting down MainWindow");
}

void MainWindow::setupUI() {
    // Create central widget with tab layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Create tab widget
    QTabWidget* tabs = new QTabWidget(this);

    // Dashboard tab
    QWidget* dashboardTab = new QWidget();
    setupDashboard();
    tabs->addTab(dashboardTab, "Dashboard");

    // Containers tab
    QWidget* containersTab = new QWidget();
    setupContainerView();
    tabs->addTab(containersTab, "Containers");

    // Network tab
    QWidget* networkTab = new QWidget();
    setupNetworkView();
    tabs->addTab(networkTab, "Networks");

    // Updates tab
    QWidget* updatesTab = new QWidget();
    setupUpdatesView();
    tabs->addTab(updatesTab, "Updates");

    // Diagnostics tab
    QWidget* diagnosticsTab = new QWidget();
    setupDiagnosticsView();
    tabs->addTab(diagnosticsTab, "Diagnostics");

    mainLayout->addWidget(tabs);
    setCentralWidget(centralWidget);
}

void MainWindow::setupMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&Settings", this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", qApp, &QApplication::quit);

    // Container menu
    QMenu* containerMenu = menuBar->addMenu("&Containers");
    containerMenu->addAction("&Refresh", this, &MainWindow::onRefreshContainers);
    containerMenu->addAction("&Start", this, &MainWindow::onStartContainer);
    containerMenu->addAction("S&top", this, &MainWindow::onStopContainer);
    containerMenu->addAction("R&estart", this, &MainWindow::onRestartContainer);

    // Update menu
    QMenu* updateMenu = menuBar->addMenu("&Updates");
    updateMenu->addAction("&Check for Updates", this, &MainWindow::onCheckUpdates);
    updateMenu->addAction("Update &Selected", this, &MainWindow::onUpdateSelected);
    updateMenu->addAction("Update &All", this, &MainWindow::onUpdateAll);

    // Diagnostics menu
    QMenu* diagnosticsMenu = menuBar->addMenu("&Diagnostics");
    diagnosticsMenu->addAction("&Run Diagnostics", this, &MainWindow::onRunDiagnostics);
    diagnosticsMenu->addAction("&Scan Ports", this, &MainWindow::onScanPorts);
    diagnosticsMenu->addAction("Check &Connectivity", this, &MainWindow::onCheckConnectivity);

    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About", [this]() {
        QMessageBox::about(this, "About",
            "Docker Homelab Manager v0.1.0\n\n"
            "A comprehensive tool for managing Docker containers\n"
            "in homelab environments.");
    });

    setMenuBar(menuBar);
}

void MainWindow::setupToolBar() {
    QToolBar* toolBar = new QToolBar(this);

    btnRefresh_ = new QPushButton("Refresh", this);
    btnStart_ = new QPushButton("Start", this);
    btnStop_ = new QPushButton("Stop", this);
    btnRestart_ = new QPushButton("Restart", this);
    btnCheckUpdates_ = new QPushButton("Check Updates", this);
    btnRunDiagnostics_ = new QPushButton("Run Diagnostics", this);

    connect(btnRefresh_, &QPushButton::clicked, this, &MainWindow::onRefreshContainers);
    connect(btnStart_, &QPushButton::clicked, this, &MainWindow::onStartContainer);
    connect(btnStop_, &QPushButton::clicked, this, &MainWindow::onStopContainer);
    connect(btnRestart_, &QPushButton::clicked, this, &MainWindow::onRestartContainer);
    connect(btnCheckUpdates_, &QPushButton::clicked, this, &MainWindow::onCheckUpdates);
    connect(btnRunDiagnostics_, &QPushButton::clicked, this, &MainWindow::onRunDiagnostics);

    toolBar->addWidget(btnRefresh_);
    toolBar->addSeparator();
    toolBar->addWidget(btnStart_);
    toolBar->addWidget(btnStop_);
    toolBar->addWidget(btnRestart_);
    toolBar->addSeparator();
    toolBar->addWidget(btnCheckUpdates_);
    toolBar->addWidget(btnRunDiagnostics_);

    addToolBar(toolBar);
}

void MainWindow::setupDashboard() {
    // TODO: Implement dashboard with statistics and overview
}

void MainWindow::setupContainerView() {
    // TODO: Implement container table view
}

void MainWindow::setupNetworkView() {
    // TODO: Implement network diagnostics view
}

void MainWindow::setupUpdatesView() {
    // TODO: Implement updates view
}

void MainWindow::setupDiagnosticsView() {
    // TODO: Implement diagnostics view
}

void MainWindow::onRefreshContainers() {
    LOG_INFO("Refreshing containers");
    statusBar()->showMessage("Refreshing containers...");

    try {
        containerManager_->refreshContainers();
        containers_ = containerManager_->getAllContainers();
        updateContainerTable();
        updateDashboard();
        statusBar()->showMessage("Containers refreshed", 3000);
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to refresh containers: ") + e.what());
        showError("Failed to refresh containers");
    }
}

void MainWindow::onStartContainer() {
    // TODO: Implement start container
}

void MainWindow::onStopContainer() {
    // TODO: Implement stop container
}

void MainWindow::onRestartContainer() {
    // TODO: Implement restart container
}

void MainWindow::onViewLogs() {
    // TODO: Implement view logs
}

void MainWindow::onCheckUpdates() {
    LOG_INFO("Checking for updates");
    statusBar()->showMessage("Checking for updates...");
    // TODO: Implement update check
}

void MainWindow::onUpdateSelected() {
    // TODO: Implement update selected
}

void MainWindow::onUpdateAll() {
    // TODO: Implement update all
}

void MainWindow::onRunDiagnostics() {
    LOG_INFO("Running diagnostics");
    statusBar()->showMessage("Running diagnostics...");
    // TODO: Implement diagnostics
}

void MainWindow::onFixIssue() {
    // TODO: Implement fix issue
}

void MainWindow::onViewIssueDetails() {
    // TODO: Implement view issue details
}

void MainWindow::onScanPorts() {
    LOG_INFO("Scanning ports");
    statusBar()->showMessage("Scanning ports...");
    // TODO: Implement port scan
}

void MainWindow::onCheckConnectivity() {
    LOG_INFO("Checking connectivity");
    // TODO: Implement connectivity check
}

void MainWindow::onResolveConflict() {
    // TODO: Implement resolve conflict
}

void MainWindow::onOpenSettings() {
    // TODO: Implement settings dialog
}

void MainWindow::onAutoRefresh() {
    // Auto-refresh in background
    onRefreshContainers();
}

void MainWindow::onAutoHealthCheck() {
    // Run health check in background
    // TODO: Implement background health check
}

void MainWindow::updateDashboard() {
    // TODO: Update dashboard statistics
}

void MainWindow::updateContainerTable() {
    // TODO: Update container table with current data
}

void MainWindow::updateIssuesList() {
    // TODO: Update issues list
}

void MainWindow::updateNetworkStatus() {
    // TODO: Update network status
}

void MainWindow::updateStatistics() {
    // TODO: Update statistics
}

void MainWindow::showError(const std::string& message) {
    QMessageBox::critical(this, "Error", QString::fromStdString(message));
}

void MainWindow::showSuccess(const std::string& message) {
    QMessageBox::information(this, "Success", QString::fromStdString(message));
}

void MainWindow::showInfo(const std::string& message) {
    QMessageBox::information(this, "Information", QString::fromStdString(message));
}

bool MainWindow::confirmAction(const std::string& message) {
    return QMessageBox::question(this, "Confirm", QString::fromStdString(message))
        == QMessageBox::Yes;
}

} // namespace ui
