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
#include <QHeaderView>
#include <QGridLayout>

namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , autoRefreshTimer_(new QTimer(this))
    , healthCheckTimer_(new QTimer(this))
{
    LOG_INFO("Initializing MainWindow");

    setWindowTitle("Docker Homelab Manager v0.1.0");
    resize(1200, 800);

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
    QVBoxLayout* dashboardLayout = new QVBoxLayout(dashboardTab);
    setupDashboard();
    dashboardLayout->addWidget(createDashboardWidget());
    tabs->addTab(dashboardTab, "Dashboard");

    // Containers tab
    QWidget* containersTab = new QWidget();
    QVBoxLayout* containersLayout = new QVBoxLayout(containersTab);
    setupContainerView();
    containersLayout->addWidget(containerTable_);
    tabs->addTab(containersTab, "Containers");

    // Network tab
    QWidget* networkTab = new QWidget();
    QVBoxLayout* networkLayout = new QVBoxLayout(networkTab);
    setupNetworkView();
    networkLayout->addWidget(networkTable_);
    tabs->addTab(networkTab, "Networks");

    // Updates tab
    QWidget* updatesTab = new QWidget();
    QVBoxLayout* updatesLayout = new QVBoxLayout(updatesTab);
    setupUpdatesView();
    updatesLayout->addWidget(updatesTable_);
    tabs->addTab(updatesTab, "Updates");

    // Diagnostics tab
    QWidget* diagnosticsTab = new QWidget();
    QVBoxLayout* diagnosticsLayout = new QVBoxLayout(diagnosticsTab);
    setupDiagnosticsView();
    diagnosticsLayout->addWidget(issuesTable_);
    tabs->addTab(diagnosticsTab, "Diagnostics");

    mainLayout->addWidget(tabs);
    setCentralWidget(centralWidget);
}

QWidget* MainWindow::createDashboardWidget() {
    QWidget* dashboard = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(dashboard);

    // Statistics group
    QGroupBox* statsGroup = new QGroupBox("System Statistics");
    QGridLayout* statsLayout = new QGridLayout();

    statsRunning_ = new QLabel("0");
    statsStopped_ = new QLabel("0");
    statsIssues_ = new QLabel("0");
    statsUpdates_ = new QLabel("0");

    QFont boldFont;
    boldFont.setBold(true);
    boldFont.setPointSize(16);

    statsRunning_->setFont(boldFont);
    statsStopped_->setFont(boldFont);
    statsIssues_->setFont(boldFont);
    statsUpdates_->setFont(boldFont);

    statsLayout->addWidget(new QLabel("Running Containers:"), 0, 0);
    statsLayout->addWidget(statsRunning_, 0, 1);
    statsLayout->addWidget(new QLabel("Stopped Containers:"), 1, 0);
    statsLayout->addWidget(statsStopped_, 1, 1);
    statsLayout->addWidget(new QLabel("Active Issues:"), 2, 0);
    statsLayout->addWidget(statsIssues_, 2, 1);
    statsLayout->addWidget(new QLabel("Available Updates:"), 3, 0);
    statsLayout->addWidget(statsUpdates_, 3, 1);

    statsGroup->setLayout(statsLayout);
    layout->addWidget(statsGroup);

    // Quick actions group
    QGroupBox* actionsGroup = new QGroupBox("Quick Actions");
    QHBoxLayout* actionsLayout = new QHBoxLayout();

    QPushButton* btnQuickRefresh = new QPushButton("Refresh All");
    QPushButton* btnQuickUpdate = new QPushButton("Check Updates");
    QPushButton* btnQuickDiag = new QPushButton("Run Diagnostics");

    connect(btnQuickRefresh, &QPushButton::clicked, this, &MainWindow::onRefreshContainers);
    connect(btnQuickUpdate, &QPushButton::clicked, this, &MainWindow::onCheckUpdates);
    connect(btnQuickDiag, &QPushButton::clicked, this, &MainWindow::onRunDiagnostics);

    actionsLayout->addWidget(btnQuickRefresh);
    actionsLayout->addWidget(btnQuickUpdate);
    actionsLayout->addWidget(btnQuickDiag);

    actionsGroup->setLayout(actionsLayout);
    layout->addWidget(actionsGroup);

    layout->addStretch();
    return dashboard;
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
            "in homelab environments.\n\n"
            "Features:\n"
            "• Container management with dependency resolution\n"
            "• Automatic update checking via Docker Hub API\n"
            "• Network diagnostics and port conflict detection\n"
            "• SQLite-based data persistence\n"
            "• AI-powered automation");
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
    // Dashboard setup handled in createDashboardWidget()
}

void MainWindow::setupContainerView() {
    containerTable_ = new QTableWidget();
    containerTable_->setColumnCount(6);
    containerTable_->setHorizontalHeaderLabels({"Name", "Image", "Status", "Ports", "Created", "Actions"});
    containerTable_->horizontalHeader()->setStretchLastSection(true);
    containerTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    containerTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    containerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    containerTable_->setAlternatingRowColors(true);

    // Set column widths
    containerTable_->setColumnWidth(0, 150);  // Name
    containerTable_->setColumnWidth(1, 200);  // Image
    containerTable_->setColumnWidth(2, 100);  // Status
    containerTable_->setColumnWidth(3, 150);  // Ports
    containerTable_->setColumnWidth(4, 150);  // Created

    LOG_INFO("Container table view configured");
}

void MainWindow::setupNetworkView() {
    networkTable_ = new QTableWidget();
    networkTable_->setColumnCount(5);
    networkTable_->setHorizontalHeaderLabels({"Container", "Port", "Protocol", "Status", "Conflict"});
    networkTable_->horizontalHeader()->setStretchLastSection(true);
    networkTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    networkTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    networkTable_->setAlternatingRowColors(true);

    // Set column widths
    networkTable_->setColumnWidth(0, 150);  // Container
    networkTable_->setColumnWidth(1, 100);  // Port
    networkTable_->setColumnWidth(2, 100);  // Protocol
    networkTable_->setColumnWidth(3, 100);  // Status

    LOG_INFO("Network table view configured");
}

void MainWindow::setupUpdatesView() {
    updatesTable_ = new QTableWidget();
    updatesTable_->setColumnCount(5);
    updatesTable_->setHorizontalHeaderLabels({"Container", "Current Version", "Latest Version", "Status", "Actions"});
    updatesTable_->horizontalHeader()->setStretchLastSection(true);
    updatesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    updatesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    updatesTable_->setAlternatingRowColors(true);

    // Set column widths
    updatesTable_->setColumnWidth(0, 150);  // Container
    updatesTable_->setColumnWidth(1, 150);  // Current
    updatesTable_->setColumnWidth(2, 150);  // Latest
    updatesTable_->setColumnWidth(3, 100);  // Status

    LOG_INFO("Updates table view configured");
}

void MainWindow::setupDiagnosticsView() {
    issuesTable_ = new QTableWidget();
    issuesTable_->setColumnCount(5);
    issuesTable_->setHorizontalHeaderLabels({"Severity", "Category", "Container", "Title", "Actions"});
    issuesTable_->horizontalHeader()->setStretchLastSection(true);
    issuesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    issuesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issuesTable_->setAlternatingRowColors(true);

    // Set column widths
    issuesTable_->setColumnWidth(0, 100);  // Severity
    issuesTable_->setColumnWidth(1, 120);  // Category
    issuesTable_->setColumnWidth(2, 150);  // Container
    issuesTable_->setColumnWidth(3, 300);  // Title

    LOG_INFO("Diagnostics table view configured");
}

void MainWindow::onRefreshContainers() {
    LOG_INFO("Refreshing containers");
    statusBar()->showMessage("Refreshing containers...");

    try {
        containerManager_->refreshContainers();
        containers_ = containerManager_->getAllContainers();

        // Save containers to database
        for (const auto& container : containers_) {
            database_->saveContainer(container);
        }

        updateContainerTable();
        updateDashboard();
        updateStatistics();

        statusBar()->showMessage(QString("Refreshed %1 containers").arg(containers_.size()), 3000);
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to refresh containers: ") + e.what());
        showError("Failed to refresh containers: " + std::string(e.what()));
        statusBar()->showMessage("Refresh failed", 3000);
    }
}

void MainWindow::onStartContainer() {
    int row = containerTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(containers_.size())) {
        showError("Please select a container");
        return;
    }

    const auto& container = containers_[row];

    if (confirmAction("Start container: " + container.name + "?")) {
        LOG_INFO("Starting container: " + container.name);
        statusBar()->showMessage("Starting container...");

        try {
            if (containerManager_->startContainer(container.id)) {
                showSuccess("Container started: " + container.name);
                onRefreshContainers();
            } else {
                showError("Failed to start container");
            }
        }
        catch (const std::exception& e) {
            showError("Error: " + std::string(e.what()));
        }
    }
}

void MainWindow::onStopContainer() {
    int row = containerTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(containers_.size())) {
        showError("Please select a container");
        return;
    }

    const auto& container = containers_[row];

    if (confirmAction("Stop container: " + container.name + "?")) {
        LOG_INFO("Stopping container: " + container.name);
        statusBar()->showMessage("Stopping container...");

        try {
            if (containerManager_->stopContainer(container.id)) {
                showSuccess("Container stopped: " + container.name);
                onRefreshContainers();
            } else {
                showError("Failed to stop container");
            }
        }
        catch (const std::exception& e) {
            showError("Error: " + std::string(e.what()));
        }
    }
}

void MainWindow::onRestartContainer() {
    int row = containerTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(containers_.size())) {
        showError("Please select a container");
        return;
    }

    const auto& container = containers_[row];

    if (confirmAction("Restart container: " + container.name + "?")) {
        LOG_INFO("Restarting container: " + container.name);
        statusBar()->showMessage("Restarting container...");

        try {
            if (containerManager_->restartContainer(container.id)) {
                showSuccess("Container restarted: " + container.name);
                onRefreshContainers();
            } else {
                showError("Failed to restart container");
            }
        }
        catch (const std::exception& e) {
            showError("Error: " + std::string(e.what()));
        }
    }
}

void MainWindow::onViewLogs() {
    int row = containerTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(containers_.size())) {
        showError("Please select a container");
        return;
    }

    const auto& container = containers_[row];
    showInfo("View logs for: " + container.name + "\n(Log viewer not yet implemented)");
}

void MainWindow::onCheckUpdates() {
    LOG_INFO("Checking for updates");
    statusBar()->showMessage("Checking for updates...");

    try {
        availableUpdates_ = updateChecker_->checkForUpdates(containers_);

        // Update the updates table
        updatesTable_->setRowCount(availableUpdates_.size());

        for (size_t i = 0; i < availableUpdates_.size(); ++i) {
            const auto& update = availableUpdates_[i];

            updatesTable_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(update.containerName)));
            updatesTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(update.currentTag)));
            updatesTable_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(update.latestTag)));

            QString status = update.updateAvailable ? "Available" : "Up to date";
            QTableWidgetItem* statusItem = new QTableWidgetItem(status);
            if (update.updateAvailable) {
                statusItem->setBackground(QColor(255, 255, 200));  // Light yellow
            }
            updatesTable_->setItem(i, 3, statusItem);

            // Add update button
            QPushButton* btnUpdate = new QPushButton("Update");
            btnUpdate->setEnabled(update.updateAvailable);
            updatesTable_->setCellWidget(i, 4, btnUpdate);

            connect(btnUpdate, &QPushButton::clicked, [this, update]() {
                if (confirmAction("Update container: " + update.containerName + "?")) {
                    statusBar()->showMessage("Updating container...");
                    if (updateChecker_->performUpdate(update, true)) {
                        showSuccess("Update completed");
                        onRefreshContainers();
                        onCheckUpdates();
                    } else {
                        showError("Update failed");
                    }
                }
            });
        }

        updateStatistics();

        int updateCount = std::count_if(availableUpdates_.begin(), availableUpdates_.end(),
                                       [](const auto& u) { return u.updateAvailable; });

        QString message = QString("Found %1 update(s)").arg(updateCount);
        statusBar()->showMessage(message, 5000);

        if (updateCount > 0) {
            showInfo(message.toStdString());
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to check updates: ") + e.what());
        showError("Failed to check for updates: " + std::string(e.what()));
    }
}

void MainWindow::onUpdateSelected() {
    int row = updatesTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(availableUpdates_.size())) {
        showError("Please select an update");
        return;
    }

    const auto& update = availableUpdates_[row];
    if (!update.updateAvailable) {
        showInfo("Container is already up to date");
        return;
    }

    if (confirmAction("Update container: " + update.containerName + "?")) {
        statusBar()->showMessage("Updating container...");
        if (updateChecker_->performUpdate(update, true)) {
            showSuccess("Update completed");
            onRefreshContainers();
            onCheckUpdates();
        } else {
            showError("Update failed");
        }
    }
}

void MainWindow::onUpdateAll() {
    std::vector<update::UpdateInfo> pendingUpdates;
    for (const auto& update : availableUpdates_) {
        if (update.updateAvailable) {
            pendingUpdates.push_back(update);
        }
    }

    if (pendingUpdates.empty()) {
        showInfo("All containers are up to date");
        return;
    }

    if (confirmAction(QString("Update %1 container(s)?").arg(pendingUpdates.size()).toStdString())) {
        statusBar()->showMessage("Updating containers...");
        if (updateChecker_->performBatchUpdate(pendingUpdates)) {
            showSuccess("All updates completed");
        } else {
            showError("Some updates failed");
        }
        onRefreshContainers();
        onCheckUpdates();
    }
}

void MainWindow::onRunDiagnostics() {
    LOG_INFO("Running diagnostics");
    statusBar()->showMessage("Running diagnostics...");

    try {
        currentIssues_ = errorDiagnostics_->runFullDiagnostics();

        // Save issues to database
        for (const auto& issue : currentIssues_) {
            database_->saveIssue(issue);
        }

        updateIssuesList();
        updateStatistics();

        QString message = QString("Found %1 issue(s)").arg(currentIssues_.size());
        statusBar()->showMessage(message, 3000);

        if (!currentIssues_.empty()) {
            showInfo(message.toStdString());
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to run diagnostics: ") + e.what());
        showError("Failed to run diagnostics: " + std::string(e.what()));
    }
}

void MainWindow::onFixIssue() {
    int row = issuesTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentIssues_.size())) {
        showError("Please select an issue");
        return;
    }

    const auto& issue = currentIssues_[row];
    showInfo("Auto-fix for: " + issue.title + "\n(Auto-fix not yet implemented)");
}

void MainWindow::onViewIssueDetails() {
    int row = issuesTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentIssues_.size())) {
        showError("Please select an issue");
        return;
    }

    const auto& issue = currentIssues_[row];
    QString details = QString("Issue Details\n\nSeverity: %1\nCategory: %2\nContainer: %3\n\nDescription:\n%4")
        .arg(QString::fromStdString(issue.severity))
        .arg(QString::fromStdString(issue.category))
        .arg(QString::fromStdString(issue.containerId))
        .arg(QString::fromStdString(issue.description));

    QMessageBox::information(this, QString::fromStdString(issue.title), details);
}

void MainWindow::onScanPorts() {
    LOG_INFO("Scanning ports");
    statusBar()->showMessage("Scanning ports...");

    try {
        auto openPorts = portScanner_->scanOpenPorts(1, 65535);

        // Update network table with port information
        networkTable_->setRowCount(0);

        for (const auto& container : containers_) {
            for (const auto& port : container.ports) {
                int row = networkTable_->rowCount();
                networkTable_->insertRow(row);

                networkTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(container.name)));
                networkTable_->setItem(row, 1, new QTableWidgetItem(QString::number(port.hostPort)));
                networkTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(port.protocol)));

                // Check for conflicts
                bool hasConflict = std::find(openPorts.begin(), openPorts.end(), port.hostPort) != openPorts.end();
                QString status = hasConflict ? "Conflict" : "OK";

                QTableWidgetItem* statusItem = new QTableWidgetItem(status);
                if (hasConflict) {
                    statusItem->setBackground(QColor(255, 200, 200));  // Light red
                }
                networkTable_->setItem(row, 3, statusItem);

                networkTable_->setItem(row, 4, new QTableWidgetItem(hasConflict ? "Yes" : "No"));
            }
        }

        statusBar()->showMessage("Port scan completed", 3000);
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Port scan failed: ") + e.what());
        showError("Port scan failed: " + std::string(e.what()));
    }
}

void MainWindow::onCheckConnectivity() {
    LOG_INFO("Checking connectivity");
    statusBar()->showMessage("Checking connectivity...");
    showInfo("Connectivity check initiated\n(Full implementation pending)");
}

void MainWindow::onResolveConflict() {
    showInfo("Conflict resolution\n(Implementation pending)");
}

void MainWindow::onOpenSettings() {
    showInfo("Settings dialog\n(Implementation pending)");
}

void MainWindow::onAutoRefresh() {
    // Silent refresh in background
    try {
        containerManager_->refreshContainers();
        containers_ = containerManager_->getAllContainers();
        updateContainerTable();
        updateDashboard();
    }
    catch (...) {
        // Silent failure for auto-refresh
    }
}

void MainWindow::onAutoHealthCheck() {
    // Background health check
    try {
        errorDiagnostics_->runFullDiagnostics();
    }
    catch (...) {
        // Silent failure
    }
}

void MainWindow::updateDashboard() {
    updateStatistics();
}

void MainWindow::updateContainerTable() {
    containerTable_->setRowCount(containers_.size());

    for (size_t i = 0; i < containers_.size(); ++i) {
        const auto& container = containers_[i];

        // Name
        containerTable_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(container.name)));

        // Image
        containerTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(container.image)));

        // Status
        QString status = (container.state == docker::ContainerState::Running) ? "Running" :
                        (container.state == docker::ContainerState::Stopped) ? "Stopped" :
                        (container.state == docker::ContainerState::Paused) ? "Paused" : "Unknown";
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        if (container.state == docker::ContainerState::Running) {
            statusItem->setBackground(QColor(200, 255, 200));  // Light green
        }
        containerTable_->setItem(i, 2, statusItem);

        // Ports
        QString portsStr;
        for (size_t j = 0; j < container.ports.size(); ++j) {
            if (j > 0) portsStr += ", ";
            portsStr += QString::number(container.ports[j].hostPort);
        }
        containerTable_->setItem(i, 3, new QTableWidgetItem(portsStr));

        // Created
        containerTable_->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(container.created)));

        // Actions (placeholder)
        containerTable_->setItem(i, 5, new QTableWidgetItem("..."));
    }

    LOG_INFO("Container table updated with " + std::to_string(containers_.size()) + " containers");
}

void MainWindow::updateIssuesList() {
    issuesTable_->setRowCount(currentIssues_.size());

    for (size_t i = 0; i < currentIssues_.size(); ++i) {
        const auto& issue = currentIssues_[i];

        // Severity
        QTableWidgetItem* severityItem = new QTableWidgetItem(QString::fromStdString(issue.severity));
        if (issue.severity == "Critical" || issue.severity == "Error") {
            severityItem->setBackground(QColor(255, 200, 200));  // Light red
        } else if (issue.severity == "Warning") {
            severityItem->setBackground(QColor(255, 255, 200));  // Light yellow
        }
        issuesTable_->setItem(i, 0, severityItem);

        // Category, Container, Title
        issuesTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(issue.category)));
        issuesTable_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(issue.containerId)));
        issuesTable_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(issue.title)));

        // Actions
        QPushButton* btnDetails = new QPushButton("Details");
        issuesTable_->setCellWidget(i, 4, btnDetails);
        connect(btnDetails, &QPushButton::clicked, this, &MainWindow::onViewIssueDetails);
    }
}

void MainWindow::updateNetworkStatus() {
    // Network status updates handled by onScanPorts
}

void MainWindow::updateStatistics() {
    int running = 0, stopped = 0;
    for (const auto& container : containers_) {
        if (container.state == docker::ContainerState::Running) {
            running++;
        } else {
            stopped++;
        }
    }

    int updatesAvailable = std::count_if(availableUpdates_.begin(), availableUpdates_.end(),
                                        [](const auto& u) { return u.updateAvailable; });

    statsRunning_->setText(QString::number(running));
    statsStopped_->setText(QString::number(stopped));
    statsIssues_->setText(QString::number(currentIssues_.size()));
    statsUpdates_->setText(QString::number(updatesAvailable));
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
