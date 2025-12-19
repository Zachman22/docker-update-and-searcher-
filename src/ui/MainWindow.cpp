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
#include <QTextEdit>
#include <QSplitter>
#include <QProgressBar>
#include <QComboBox>

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
    dependencyResolver_ = std::make_shared<docker::DependencyResolver>();
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
    setWindowTitle("Docker Homelab Manager v0.3.0");
    resize(1200, 800);

    // Create central widget with tab layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Create tab widget
    QTabWidget* tabs = new QTabWidget(this);

    // Dashboard tab
    QWidget* dashboardTab = new QWidget();
    QVBoxLayout* dashboardLayout = new QVBoxLayout(dashboardTab);
    setupDashboard();
    tabs->addTab(dashboardTab, "📊 Dashboard");

    // Containers tab
    QWidget* containersTab = new QWidget();
    QVBoxLayout* containersLayout = new QVBoxLayout(containersTab);
    setupContainerView();
    if (containerTable_) {
        containersLayout->addWidget(containerTable_);
    }
    tabs->addTab(containersTab, "🐳 Containers");

    // Network tab
    QWidget* networkTab = new QWidget();
    QVBoxLayout* networkLayout = new QVBoxLayout(networkTab);
    setupNetworkView();
    tabs->addTab(networkTab, "🌐 Network");

    // Updates tab
    QWidget* updatesTab = new QWidget();
    QVBoxLayout* updatesLayout = new QVBoxLayout(updatesTab);
    setupUpdatesView();
    tabs->addTab(updatesTab, "🔄 Updates");

    // Diagnostics tab
    QWidget* diagnosticsTab = new QWidget();
    QVBoxLayout* diagnosticsLayout = new QVBoxLayout(diagnosticsTab);
    setupDiagnosticsView();
    tabs->addTab(diagnosticsTab, "🔧 Diagnostics");

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
            "Docker Homelab Manager v0.3.0\n\n"
            "A comprehensive tool for managing Docker containers\n"
            "in homelab environments.\n\n"
            "Features:\n"
            "• Container management\n"
            "• Network diagnostics\n"
            "• Dependency resolution\n"
            "• Error diagnosis & auto-fix\n"
            "• Safe updates with rollback");
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
    // Create dashboard layout with statistics
    QGroupBox* statsGroup = new QGroupBox("System Statistics", this);
    QGridLayout* statsLayout = new QGridLayout(statsGroup);

    // Create stat labels
    statsRunning_ = new QLabel("0", this);
    statsStopped_ = new QLabel("0", this);
    statsIssues_ = new QLabel("0", this);
    statsUpdates_ = new QLabel("0", this);

    // Style stat labels
    QString statStyle = "font-size: 24px; font-weight: bold; padding: 10px;";
    statsRunning_->setStyleSheet(statStyle + "color: #28a745;");
    statsStopped_->setStyleSheet(statStyle + "color: #dc3545;");
    statsIssues_->setStyleSheet(statStyle + "color: #ffc107;");
    statsUpdates_->setStyleSheet(statStyle + "color: #17a2b8;");

    // Add to layout
    QLabel* runningLabel = new QLabel("Running Containers:", this);
    QLabel* stoppedLabel = new QLabel("Stopped Containers:", this);
    QLabel* issuesLabel = new QLabel("Active Issues:", this);
    QLabel* updatesLabel = new QLabel("Available Updates:", this);

    statsLayout->addWidget(runningLabel, 0, 0);
    statsLayout->addWidget(statsRunning_, 0, 1);
    statsLayout->addWidget(stoppedLabel, 0, 2);
    statsLayout->addWidget(statsStopped_, 0, 3);
    statsLayout->addWidget(issuesLabel, 1, 0);
    statsLayout->addWidget(statsIssues_, 1, 1);
    statsLayout->addWidget(updatesLabel, 1, 2);
    statsLayout->addWidget(statsUpdates_, 1, 3);
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
    // Create network diagnostics table
    networkTable_ = new QTableWidget(this);
    networkTable_->setColumnCount(5);
    QStringList headers;
    headers << "Test Type" << "Source" << "Destination" << "Status" << "Details";
    networkTable_->setHorizontalHeaderLabels(headers);

    networkTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    networkTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    networkTable_->horizontalHeader()->setStretchLastSection(true);

    // Add control buttons
    QPushButton* btnTestConnectivity = new QPushButton("Test Container Connectivity", this);
    QPushButton* btnTestInternet = new QPushButton("Test Internet Access", this);
    QPushButton* btnTestDNS = new QPushButton("Test DNS Resolution", this);
    QPushButton* btnScanNetworks = new QPushButton("Scan All Networks", this);

    connect(btnTestConnectivity, &QPushButton::clicked, [this]() {
        statusBar()->showMessage("Testing container connectivity...");
        networkTable_->setRowCount(0);

        // Test connectivity between all running containers
        int row = 0;
        for (size_t i = 0; i < containers_.size(); ++i) {
            for (size_t j = i + 1; j < containers_.size(); ++j) {
                if (containers_[i].isRunning() && containers_[j].isRunning()) {
                    auto test = networkDiagnostics_->testContainerConnectivity(
                        containers_[i].getId(), containers_[j].getId());

                    networkTable_->insertRow(row);
                    networkTable_->setItem(row, 0, new QTableWidgetItem("Container→Container"));
                    networkTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(containers_[i].getName())));
                    networkTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(containers_[j].getName())));

                    QString status = (test.status == network::ConnectivityStatus::Success) ? "✓ Success" : "✗ Failed";
                    QTableWidgetItem* statusItem = new QTableWidgetItem(status);
                    statusItem->setForeground(QBrush(QColor(test.status == network::ConnectivityStatus::Success ? Qt::green : Qt::red)));
                    networkTable_->setItem(row, 3, statusItem);

                    QString details = QString::fromStdString(test.errorMessage.empty() ?
                        "Latency: " + std::to_string(test.latency.count()) + "ms" :
                        test.errorMessage);
                    networkTable_->setItem(row, 4, new QTableWidgetItem(details));
                    row++;
                }
            }
        }
        statusBar()->showMessage("Connectivity test complete", 3000);
    });

    connect(btnTestInternet, &QPushButton::clicked, [this]() {
        statusBar()->showMessage("Testing internet connectivity...");
        networkTable_->setRowCount(0);

        int row = 0;
        for (const auto& container : containers_) {
            if (container.isRunning()) {
                auto test = networkDiagnostics_->testInternetConnectivity(container.getId());

                networkTable_->insertRow(row);
                networkTable_->setItem(row, 0, new QTableWidgetItem("Internet Access"));
                networkTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(container.getName())));
                networkTable_->setItem(row, 2, new QTableWidgetItem("8.8.8.8"));

                QString status = (test.status == network::ConnectivityStatus::Success) ? "✓ Connected" : "✗ No Access";
                QTableWidgetItem* statusItem = new QTableWidgetItem(status);
                statusItem->setForeground(QBrush(QColor(test.status == network::ConnectivityStatus::Success ? Qt::green : Qt::red)));
                networkTable_->setItem(row, 3, statusItem);

                networkTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(test.errorMessage)));
                row++;
            }
        }
        statusBar()->showMessage("Internet test complete", 3000);
    });

    connect(btnTestDNS, &QPushButton::clicked, [this]() {
        statusBar()->showMessage("Testing DNS resolution...");
        networkTable_->setRowCount(0);

        std::vector<std::string> testDomains = {"google.com", "docker.io", "github.com"};
        int row = 0;

        for (const auto& container : containers_) {
            if (container.isRunning()) {
                for (const auto& domain : testDomains) {
                    auto test = networkDiagnostics_->testContainerDNS(container.getId(), domain);

                    networkTable_->insertRow(row);
                    networkTable_->setItem(row, 0, new QTableWidgetItem("DNS Resolution"));
                    networkTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(container.getName())));
                    networkTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(domain)));

                    QString status = test.success ? "✓ Resolved" : "✗ Failed";
                    QTableWidgetItem* statusItem = new QTableWidgetItem(status);
                    statusItem->setForeground(QBrush(QColor(test.success ? Qt::green : Qt::red)));
                    networkTable_->setItem(row, 3, statusItem);

                    QString details = test.success ?
                        QString("IPs: %1 (%2ms)").arg(test.resolvedIPs.size()).arg(test.responseTime.count()) :
                        QString::fromStdString(test.errorMessage);
                    networkTable_->setItem(row, 4, new QTableWidgetItem(details));
                    row++;
                }
            }
        }
        statusBar()->showMessage("DNS test complete", 3000);
    });

    LOG_INFO("Network diagnostics view initialized");
}

void MainWindow::setupUpdatesView() {
    // Create updates table
    updatesTable_ = new QTableWidget(this);
    updatesTable_->setColumnCount(6);
    QStringList headers;
    headers << "Container" << "Current Tag" << "Latest Tag" << "Current Digest" << "Latest Digest" << "Status";
    updatesTable_->setHorizontalHeaderLabels(headers);

    updatesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    updatesTable_->setSelectionMode(QAbstractItemView::MultiSelection);
    updatesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    updatesTable_->horizontalHeader()->setStretchLastSection(true);

    // Add update buttons
    QPushButton* btnUpdate = new QPushButton("Update Selected", this);
    btnUpdateAll_ = new QPushButton("Update All", this);
    QPushButton* btnRollback = new QPushButton("Rollback Last Update", this);

    QComboBox* strategyCombo = new QComboBox(this);
    strategyCombo->addItem("Conservative", static_cast<int>(update::UpdateStrategy::Conservative));
    strategyCombo->addItem("Moderate", static_cast<int>(update::UpdateStrategy::Moderate));
    strategyCombo->addItem("Aggressive", static_cast<int>(update::UpdateStrategy::Aggressive));
    strategyCombo->setCurrentIndex(1); // Default to Moderate

    connect(strategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, strategyCombo](int index) {
        auto strategy = static_cast<update::UpdateStrategy>(strategyCombo->itemData(index).toInt());
        updateChecker_->setUpdateStrategy(strategy);
        LOG_INFO("Update strategy changed");
    });

    connect(btnUpdate, &QPushButton::clicked, this, &MainWindow::onUpdateSelected);
    connect(btnUpdateAll_, &QPushButton::clicked, this, &MainWindow::onUpdateAll);
    connect(btnRollback, &QPushButton::clicked, [this]() {
        if (!updatesTable_->selectedItems().isEmpty()) {
            int row = updatesTable_->selectedItems()[0]->row();
            QString containerId = updatesTable_->item(row, 0)->data(Qt::UserRole).toString();

            if (confirmAction("Rollback last update for this container?")) {
                statusBar()->showMessage("Rolling back...");
                if (updateChecker_->rollbackUpdate(containerId.toStdString())) {
                    showSuccess("Rollback successful");
                    onRefreshContainers();
                } else {
                    showError("Rollback failed - check logs");
                }
            }
        }
    });

    LOG_INFO("Updates view initialized");
}

void MainWindow::setupDiagnosticsView() {
    // Create diagnostics table
    issuesTable_ = new QTableWidget(this);
    issuesTable_->setColumnCount(5);
    QStringList headers;
    headers << "Severity" << "Category" << "Title" << "Container" << "Detected At";
    issuesTable_->setHorizontalHeaderLabels(headers);

    issuesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    issuesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issuesTable_->horizontalHeader()->setStretchLastSection(true);

    // Details text area
    QTextEdit* detailsText = new QTextEdit(this);
    detailsText->setReadOnly(true);
    detailsText->setMaximumHeight(200);

    // Connect selection to details
    connect(issuesTable_, &QTableWidget::itemSelectionChanged, [this, detailsText]() {
        auto selected = issuesTable_->selectedItems();
        if (!selected.isEmpty()) {
            int row = selected[0]->row();
            if (row < static_cast<int>(currentIssues_.size())) {
                const auto& issue = currentIssues_[row];

                QString details;
                details += "Description:\n" + QString::fromStdString(issue.description) + "\n\n";
                details += "Suggested Fixes:\n";
                for (const auto& fix : issue.suggestedFixes) {
                    details += "• " + QString::fromStdString(fix) + "\n";
                }

                if (issue.autoFixAvailable) {
                    details += "\n✓ Auto-fix available";
                }

                detailsText->setText(details);
            }
        }
    });

    // Add control buttons
    QPushButton* btnAutoFix = new QPushButton("Auto-Fix Selected", this);
    QPushButton* btnCheckHealth = new QPushButton("Check System Health", this);
    QPushButton* btnCheckDeps = new QPushButton("Check Dependencies", this);

    connect(btnAutoFix, &QPushButton::clicked, [this]() {
        auto selected = issuesTable_->selectedItems();
        if (!selected.isEmpty()) {
            int row = selected[0]->row();
            if (row < static_cast<int>(currentIssues_.size())) {
                const auto& issue = currentIssues_[row];
                if (errorDiagnostics_->attemptAutoFix(issue)) {
                    showSuccess("Auto-fix successful");
                    onRunDiagnostics();
                } else {
                    showError("Auto-fix not available or failed");
                }
            }
        }
    });

    connect(btnCheckHealth, &QPushButton::clicked, [this]() {
        statusBar()->showMessage("Checking system health...");
        auto health = errorDiagnostics_->checkSystemHealth();

        QString message;
        message += "System Health: " + QString(health.healthy ? "✓ Healthy" : "✗ Issues Detected") + "\n\n";
        message += QString::fromStdString(health.message) + "\n\n";
        message += "Issues found: " + QString::number(health.issues.size());

        QMessageBox::information(this, "System Health", message);
        statusBar()->showMessage(health.healthy ? "System healthy" : "Issues detected", 3000);
    });

    connect(btnCheckDeps, &QPushButton::clicked, [this]() {
        statusBar()->showMessage("Checking dependencies...");

        QString report;
        for (const auto& container : containers_) {
            auto deps = dependencyResolver_->analyzeDependencies(container.getId());
            if (!deps.empty()) {
                report += QString::fromStdString(container.getName()) + ":\n";
                for (const auto& dep : deps) {
                    report += "  • " + QString::fromStdString(dep.details) + "\n";
                }
                report += "\n";
            }
        }

        // Check for circular dependencies
        if (dependencyResolver_->hasCircularDependencies()) {
            report += "\n⚠ WARNING: Circular dependencies detected!\n";
        }

        QMessageBox::information(this, "Dependency Analysis", report.isEmpty() ? "No dependencies found" : report);
        statusBar()->showMessage("Dependency check complete", 3000);
    });

    LOG_INFO("Diagnostics view initialized");
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

    QList<QTableWidgetItem*> selected = containerTable_->selectedItems();
    if (selected.isEmpty()) {
        showInfo("Please select a container to start");
        return;
    }

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
        bool success = dockerClient_->startContainer(containerId.toStdString());

        if (success) {
            LOG_INFO("Container started successfully: " + containerName.toStdString());
            statusBar()->showMessage("Container " + containerName + " started", 3000);
            showSuccess("Container '" + containerName.toStdString() + "' started successfully");
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

    QList<QTableWidgetItem*> selected = containerTable_->selectedItems();
    if (selected.isEmpty()) {
        showInfo("Please select a container to stop");
        return;
    }

    int row = selected[0]->row();
    QTableWidgetItem* nameItem = containerTable_->item(row, 0);
    if (!nameItem) return;

    QString containerId = nameItem->data(Qt::UserRole).toString();
    QString containerName = nameItem->text();

    if (containerId.isEmpty()) {
        showError("Invalid container ID");
        return;
    }

    if (!confirmAction("Are you sure you want to stop container '" + containerName.toStdString() + "'?")) {
        return;
    }

    LOG_INFO("Stopping container: " + containerName.toStdString());
    statusBar()->showMessage("Stopping container " + containerName + "...");

    try {
        bool success = dockerClient_->stopContainer(containerId.toStdString(), 10);

        if (success) {
            LOG_INFO("Container stopped successfully: " + containerName.toStdString());
            statusBar()->showMessage("Container " + containerName + " stopped", 3000);
            showSuccess("Container '" + containerName.toStdString() + "' stopped successfully");
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

    QList<QTableWidgetItem*> selected = containerTable_->selectedItems();
    if (selected.isEmpty()) {
        showInfo("Please select a container to restart");
        return;
    }

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
        bool success = dockerClient_->restartContainer(containerId.toStdString());

        if (success) {
            LOG_INFO("Container restarted successfully: " + containerName.toStdString());
            statusBar()->showMessage("Container " + containerName + " restarted", 3000);
            showSuccess("Container '" + containerName.toStdString() + "' restarted successfully");
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
    // TODO: Implement view logs dialog
}

void MainWindow::onCheckUpdates() {
    LOG_INFO("Checking for updates");
    statusBar()->showMessage("Checking for updates...");

    try {
        availableUpdates_ = updateChecker_->checkForUpdates(containers_);

        // Update the updates table
        updatesTable_->setRowCount(0);
        int row = 0;

        for (const auto& update : availableUpdates_) {
            updatesTable_->insertRow(row);

            QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(update.containerName));
            nameItem->setData(Qt::UserRole, QString::fromStdString(update.containerId));
            updatesTable_->setItem(row, 0, nameItem);

            updatesTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(update.currentTag)));
            updatesTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(update.latestTag)));

            QString currentDigest = QString::fromStdString(update.currentDigest);
            if (currentDigest.length() > 12) currentDigest = currentDigest.left(12) + "...";
            updatesTable_->setItem(row, 3, new QTableWidgetItem(currentDigest));

            QString latestDigest = QString::fromStdString(update.latestDigest);
            if (latestDigest.length() > 12) latestDigest = latestDigest.left(12) + "...";
            updatesTable_->setItem(row, 4, new QTableWidgetItem(latestDigest));

            QTableWidgetItem* statusItem = new QTableWidgetItem("🔄 Update Available");
            statusItem->setForeground(QBrush(QColor(0, 150, 200)));
            updatesTable_->setItem(row, 5, statusItem);

            row++;
        }

        updateDashboard();
        statusBar()->showMessage(QString("Found %1 updates").arg(availableUpdates_.size()), 3000);
        showInfo("Found " + std::to_string(availableUpdates_.size()) + " available updates");
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to check updates: ") + e.what());
        showError("Failed to check updates");
    }
}

void MainWindow::onUpdateSelected() {
    if (!updatesTable_ || updatesTable_->selectedItems().isEmpty()) {
        showInfo("Please select updates to apply");
        return;
    }

    std::vector<update::UpdateInfo> selectedUpdates;
    QSet<int> selectedRows;

    for (auto* item : updatesTable_->selectedItems()) {
        selectedRows.insert(item->row());
    }

    for (int row : selectedRows) {
        if (row < static_cast<int>(availableUpdates_.size())) {
            selectedUpdates.push_back(availableUpdates_[row]);
        }
    }

    if (!confirmAction("Update " + std::to_string(selectedUpdates.size()) + " container(s)?\n\nBackup will be created automatically.")) {
        return;
    }

    statusBar()->showMessage("Updating containers...");

    for (const auto& update : selectedUpdates) {
        LOG_INFO("Updating: " + update.containerName);
        if (updateChecker_->performUpdate(update, true)) {
            LOG_INFO("Update successful: " + update.containerName);
        } else {
            LOG_ERROR("Update failed: " + update.containerName);
        }
    }

    showSuccess("Update process complete - check logs for details");
    onRefreshContainers();
    onCheckUpdates();
}

void MainWindow::onUpdateAll() {
    if (availableUpdates_.empty()) {
        showInfo("No updates available");
        return;
    }

    if (!confirmAction("Update ALL " + std::to_string(availableUpdates_.size()) + " container(s)?\n\nBackup will be created for each.")) {
        return;
    }

    statusBar()->showMessage("Performing batch update...");

    bool success = updateChecker_->performBatchUpdate(availableUpdates_);

    if (success) {
        showSuccess("All updates completed successfully");
    } else {
        showError("Some updates failed - check logs");
    }

    onRefreshContainers();
    onCheckUpdates();
}

void MainWindow::onRunDiagnostics() {
    LOG_INFO("Running diagnostics");
    statusBar()->showMessage("Running full diagnostics...");

    try {
        currentIssues_ = errorDiagnostics_->runFullDiagnostics();

        // Update the issues table
        issuesTable_->setRowCount(0);
        int row = 0;

        for (const auto& issue : currentIssues_) {
            issuesTable_->insertRow(row);

            // Severity with color
            QString severityStr;
            QColor severityColor;
            switch (issue.severity) {
                case diagnostics::ErrorSeverity::Critical:
                    severityStr = "🔴 Critical";
                    severityColor = QColor(220, 53, 69);
                    break;
                case diagnostics::ErrorSeverity::Error:
                    severityStr = "🟠 Error";
                    severityColor = QColor(255, 193, 7);
                    break;
                case diagnostics::ErrorSeverity::Warning:
                    severityStr = "🟡 Warning";
                    severityColor = QColor(255, 235, 59);
                    break;
                default:
                    severityStr = "ℹ️ Info";
                    severityColor = QColor(23, 162, 184);
            }

            QTableWidgetItem* severityItem = new QTableWidgetItem(severityStr);
            severityItem->setForeground(QBrush(severityColor));
            issuesTable_->setItem(row, 0, severityItem);

            // Category
            QString categoryStr;
            switch (issue.category) {
                case diagnostics::ErrorCategory::Network: categoryStr = "Network"; break;
                case diagnostics::ErrorCategory::Storage: categoryStr = "Storage"; break;
                case diagnostics::ErrorCategory::Permission: categoryStr = "Permission"; break;
                case diagnostics::ErrorCategory::Resource: categoryStr = "Resource"; break;
                case diagnostics::ErrorCategory::Configuration: categoryStr = "Configuration"; break;
                case diagnostics::ErrorCategory::Dependency: categoryStr = "Dependency"; break;
                case diagnostics::ErrorCategory::Registry: categoryStr = "Registry"; break;
                default: categoryStr = "Unknown";
            }
            issuesTable_->setItem(row, 1, new QTableWidgetItem(categoryStr));

            // Title
            issuesTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(issue.title)));

            // Container
            issuesTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(issue.containerName)));

            // Detected At
            issuesTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(issue.detectedAt)));

            row++;
        }

        updateDashboard();
        statusBar()->showMessage(QString("Found %1 issues").arg(currentIssues_.size()), 3000);
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to run diagnostics: ") + e.what());
        showError("Failed to run diagnostics");
    }
}

void MainWindow::onFixIssue() {
    // Handled by auto-fix button in diagnostics view
}

void MainWindow::onViewIssueDetails() {
    // Handled by selection in diagnostics table
}

void MainWindow::onScanPorts() {
    LOG_INFO("Scanning ports");
    statusBar()->showMessage("Scanning open ports...");

    try {
        auto ports = portScanner_->scanOpenPorts();

        QString report;
        report += "Open Ports Detected: " + QString::number(ports.size()) + "\n\n";

        for (const auto& port : ports) {
            report += QString("Port %1/%2 - %3 (PID: %4)\n")
                .arg(port.port)
                .arg(QString::fromStdString(port.protocol))
                .arg(QString::fromStdString(port.processName))
                .arg(port.processId);
        }

        QMessageBox::information(this, "Port Scan Results", report);
        statusBar()->showMessage("Port scan complete", 3000);
    }
    catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to scan ports: ") + e.what());
        showError("Failed to scan ports");
    }
}

void MainWindow::onCheckConnectivity() {
    LOG_INFO("Checking connectivity");
    onCheckUpdates(); // Trigger network tab operations
}

void MainWindow::onResolveConflict() {
    // TODO: Implement conflict resolution
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
    auto health = errorDiagnostics_->checkSystemHealth();
    if (!health.healthy) {
        LOG_WARNING("System health check detected issues");
    }
}

void MainWindow::updateDashboard() {
    // Count running/stopped containers
    int running = 0, stopped = 0;
    for (const auto& container : containers_) {
        if (container.isRunning()) {
            running++;
        } else {
            stopped++;
        }
    }

    if (statsRunning_) statsRunning_->setText(QString::number(running));
    if (statsStopped_) statsStopped_->setText(QString::number(stopped));
    if (statsIssues_) statsIssues_->setText(QString::number(currentIssues_.size()));
    if (statsUpdates_) statsUpdates_->setText(QString::number(availableUpdates_.size()));
}

void MainWindow::updateContainerTable() {
    if (!containerTable_) return;

    LOG_INFO("Updating container table with " + std::to_string(containers_.size()) + " containers");

    containerTable_->setSortingEnabled(false);
    containerTable_->setRowCount(0);

    int row = 0;
    for (const auto& container : containers_) {
        containerTable_->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(container.getName()));
        containerTable_->setItem(row, 0, nameItem);

        QString imageStr = QString::fromStdString(container.getImage());
        QTableWidgetItem* imageItem = new QTableWidgetItem(imageStr);
        containerTable_->setItem(row, 1, imageItem);

        QString stateStr = QString::fromStdString(container.getStateString());
        QTableWidgetItem* stateItem = new QTableWidgetItem(stateStr);

        if (container.isRunning()) {
            stateItem->setForeground(QBrush(QColor(0, 150, 0)));
        } else {
            stateItem->setForeground(QBrush(QColor(200, 0, 0)));
        }
        containerTable_->setItem(row, 2, stateItem);

        QTableWidgetItem* statusItem = new QTableWidgetItem(QString::fromStdString(container.getStatus()));
        containerTable_->setItem(row, 3, statusItem);

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

        QString idStr = QString::fromStdString(container.getId());
        if (idStr.length() > 12) {
            idStr = idStr.left(12);
        }
        QTableWidgetItem* idItem = new QTableWidgetItem(idStr);
        containerTable_->setItem(row, 5, idItem);

        nameItem->setData(Qt::UserRole, QString::fromStdString(container.getId()));

        row++;
    }

    containerTable_->setSortingEnabled(true);
    LOG_INFO("Container table updated successfully");
}

void MainWindow::updateIssuesList() {
    // Issues are updated in onRunDiagnostics
}

void MainWindow::updateNetworkStatus() {
    // Network status is updated on-demand in network tab
}

void MainWindow::updateStatistics() {
    updateDashboard();
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
