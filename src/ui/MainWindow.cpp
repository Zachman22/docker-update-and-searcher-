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
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QBrush>
#include <QColor>

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
    QVBoxLayout* containersLayout = new QVBoxLayout(containersTab);
    setupContainerView();
    if (containerTable_) {
        containersLayout->addWidget(containerTable_);
    }
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
    // Create container table
    containerTable_ = new QTableWidget(this);

    // Set up columns
    containerTable_->setColumnCount(6);
    QStringList headers;
    headers << "Name" << "Image" << "State" << "Status" << "Ports" << "ID";
    containerTable_->setHorizontalHeaderLabels(headers);

    // Configure table behavior
    containerTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    containerTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    containerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    containerTable_->setSortingEnabled(true);

    // Set column widths
    containerTable_->setColumnWidth(0, 150); // Name
    containerTable_->setColumnWidth(1, 200); // Image
    containerTable_->setColumnWidth(2, 80);  // State
    containerTable_->setColumnWidth(3, 150); // Status
    containerTable_->setColumnWidth(4, 120); // Ports
    containerTable_->setColumnWidth(5, 120); // ID

    // Stretch last column
    containerTable_->horizontalHeader()->setStretchLastSection(true);

    LOG_INFO("Container table view initialized");
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
    if (!containerTable_) return;

    // Get selected row
    QList<QTableWidgetItem*> selected = containerTable_->selectedItems();
    if (selected.isEmpty()) {
        showInfo("Please select a container to start");
        return;
    }

    // Get container ID from first column's user data
    int row = selected[0]->row();
    QTableWidgetItem* nameItem = containerTable_->item(row, 0);
    if (!nameItem) return;

    QString containerId = nameItem->data(Qt::UserRole).toString();
    QString containerName = nameItem->text();

    if (containerId.isEmpty()) {
        showError("Invalid container ID");
        return;
    }

    LOG_INFO("Starting container: " + containerName.toStdString());
    statusBar()->showMessage("Starting container " + containerName + "...");

    try {
        // Start the container
        bool success = dockerClient_->startContainer(containerId.toStdString());

        if (success) {
            LOG_INFO("Container started successfully: " + containerName.toStdString());
            statusBar()->showMessage("Container " + containerName + " started", 3000);
            showSuccess("Container '" + containerName.toStdString() + "' started successfully");

            // Refresh the container list to show updated state
            QTimer::singleShot(500, this, &MainWindow::onRefreshContainers);
        } else {
            LOG_ERROR("Failed to start container: " + containerName.toStdString());
            showError("Failed to start container '" + containerName.toStdString() + "'");
            statusBar()->showMessage("Failed to start container", 3000);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception starting container: ") + e.what());
        showError("Error starting container: " + std::string(e.what()));
        statusBar()->showMessage("Error", 3000);
    }
}

void MainWindow::onStopContainer() {
    if (!containerTable_) return;

    // Get selected row
    QList<QTableWidgetItem*> selected = containerTable_->selectedItems();
    if (selected.isEmpty()) {
        showInfo("Please select a container to stop");
        return;
    }

    // Get container ID from first column's user data
    int row = selected[0]->row();
    QTableWidgetItem* nameItem = containerTable_->item(row, 0);
    if (!nameItem) return;

    QString containerId = nameItem->data(Qt::UserRole).toString();
    QString containerName = nameItem->text();

    if (containerId.isEmpty()) {
        showError("Invalid container ID");
        return;
    }

    // Confirm before stopping
    if (!confirmAction("Are you sure you want to stop container '" + containerName.toStdString() + "'?")) {
        return;
    }

    LOG_INFO("Stopping container: " + containerName.toStdString());
    statusBar()->showMessage("Stopping container " + containerName + "...");

    try {
        // Stop the container (10 second timeout)
        bool success = dockerClient_->stopContainer(containerId.toStdString(), 10);

        if (success) {
            LOG_INFO("Container stopped successfully: " + containerName.toStdString());
            statusBar()->showMessage("Container " + containerName + " stopped", 3000);
            showSuccess("Container '" + containerName.toStdString() + "' stopped successfully");

            // Refresh the container list to show updated state
            QTimer::singleShot(500, this, &MainWindow::onRefreshContainers);
        } else {
            LOG_ERROR("Failed to stop container: " + containerName.toStdString());
            showError("Failed to stop container '" + containerName.toStdString() + "'");
            statusBar()->showMessage("Failed to stop container", 3000);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception stopping container: ") + e.what());
        showError("Error stopping container: " + std::string(e.what()));
        statusBar()->showMessage("Error", 3000);
    }
}

void MainWindow::onRestartContainer() {
    if (!containerTable_) return;

    // Get selected row
    QList<QTableWidgetItem*> selected = containerTable_->selectedItems();
    if (selected.isEmpty()) {
        showInfo("Please select a container to restart");
        return;
    }

    // Get container ID from first column's user data
    int row = selected[0]->row();
    QTableWidgetItem* nameItem = containerTable_->item(row, 0);
    if (!nameItem) return;

    QString containerId = nameItem->data(Qt::UserRole).toString();
    QString containerName = nameItem->text();

    if (containerId.isEmpty()) {
        showError("Invalid container ID");
        return;
    }

    LOG_INFO("Restarting container: " + containerName.toStdString());
    statusBar()->showMessage("Restarting container " + containerName + "...");

    try {
        // Restart the container
        bool success = dockerClient_->restartContainer(containerId.toStdString());

        if (success) {
            LOG_INFO("Container restarted successfully: " + containerName.toStdString());
            statusBar()->showMessage("Container " + containerName + " restarted", 3000);
            showSuccess("Container '" + containerName.toStdString() + "' restarted successfully");

            // Refresh the container list to show updated state
            QTimer::singleShot(1000, this, &MainWindow::onRefreshContainers);
        } else {
            LOG_ERROR("Failed to restart container: " + containerName.toStdString());
            showError("Failed to restart container '" + containerName.toStdString() + "'");
            statusBar()->showMessage("Failed to restart container", 3000);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception restarting container: ") + e.what());
        showError("Error restarting container: " + std::string(e.what()));
        statusBar()->showMessage("Error", 3000);
    }
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
    if (!containerTable_) return;

    LOG_INFO("Updating container table with " + std::to_string(containers_.size()) + " containers");

    // Disable sorting while updating
    containerTable_->setSortingEnabled(false);

    // Clear existing rows
    containerTable_->setRowCount(0);

    // Add rows for each container
    int row = 0;
    for (const auto& container : containers_) {
        containerTable_->insertRow(row);

        // Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(container.getName()));
        containerTable_->setItem(row, 0, nameItem);

        // Image
        QString imageStr = QString::fromStdString(container.getImage());
        QTableWidgetItem* imageItem = new QTableWidgetItem(imageStr);
        containerTable_->setItem(row, 1, imageItem);

        // State
        QString stateStr = QString::fromStdString(container.getStateString());
        QTableWidgetItem* stateItem = new QTableWidgetItem(stateStr);

        // Color code by state
        if (container.isRunning()) {
            stateItem->setForeground(QBrush(QColor(0, 150, 0))); // Green
        } else {
            stateItem->setForeground(QBrush(QColor(200, 0, 0))); // Red
        }
        containerTable_->setItem(row, 2, stateItem);

        // Status
        QTableWidgetItem* statusItem = new QTableWidgetItem(QString::fromStdString(container.getStatus()));
        containerTable_->setItem(row, 3, statusItem);

        // Ports
        QString portsStr;
        auto ports = container.getPorts();
        for (size_t i = 0; i < ports.size(); ++i) {
            if (i > 0) portsStr += ", ";
            if (ports[i].hostPort > 0) {
                portsStr += QString::number(ports[i].hostPort) + "->" + QString::number(ports[i].containerPort);
            } else {
                portsStr += QString::number(ports[i].containerPort);
            }
        }
        QTableWidgetItem* portsItem = new QTableWidgetItem(portsStr);
        containerTable_->setItem(row, 4, portsItem);

        // ID (short version)
        QString idStr = QString::fromStdString(container.getId());
        if (idStr.length() > 12) {
            idStr = idStr.left(12);
        }
        QTableWidgetItem* idItem = new QTableWidgetItem(idStr);
        containerTable_->setItem(row, 5, idItem);

        // Store full ID in row data for later retrieval
        nameItem->setData(Qt::UserRole, QString::fromStdString(container.getId()));

        row++;
    }

    // Re-enable sorting
    containerTable_->setSortingEnabled(true);

    LOG_INFO("Container table updated successfully");
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
