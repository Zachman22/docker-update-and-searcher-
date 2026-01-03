#include "ui/ContainerWizard.h"
#include "docker/DockerClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>

namespace ui {

// ============================================================================
// ContainerWizard
// ============================================================================

ContainerWizard::ContainerWizard(std::shared_ptr<docker::DockerClient> dockerClient,
                                 QWidget* parent)
    : QWizard(parent)
    , dockerClient_(dockerClient)
{
    setWindowTitle("Create New Container");
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::HaveHelpButton, false);

    // Add pages
    addPage(new BasicInfoPage(dockerClient_, this));
    addPage(new NetworkConfigPage(dockerClient_, this));
    addPage(new VolumeConfigPage(dockerClient_, this));
    addPage(new EnvironmentPage(this));
    addPage(new ResourcesPage(this));
    addPage(new AdvancedOptionsPage(this));
    addPage(new HealthCheckPage(this));
    addPage(new SummaryPage(this));

    resize(800, 600);
}

ContainerWizard::~ContainerWizard() = default;

nlohmann::json ContainerWizard::getContainerConfig() const {
    nlohmann::json config;

    // Collect configuration from all pages
    config["Image"] = field("image").toString().toStdString();
    config["Hostname"] = field("hostname").toString().toStdString();

    // Container name is handled separately

    // Add other configurations...

    return config;
}

std::string ContainerWizard::getContainerName() const {
    return field("containerName").toString().toStdString();
}

// ============================================================================
// BasicInfoPage
// ============================================================================

BasicInfoPage::BasicInfoPage(std::shared_ptr<docker::DockerClient> dockerClient,
                             QWidget* parent)
    : QWizardPage(parent)
    , dockerClient_(dockerClient)
{
    setTitle("Basic Information");
    setSubTitle("Configure the container's basic settings");

    auto* layout = new QFormLayout(this);

    // Container name
    nameEdit_ = new QLineEdit();
    nameEdit_->setPlaceholderText("my-container");
    layout->addRow("Container Name:", nameEdit_);
    registerField("containerName*", nameEdit_);

    // Image selection
    auto* imageLayout = new QHBoxLayout();
    imageCombo_ = new QComboBox();
    imageCombo_->setEditable(true);
    imageCombo_->setPlaceholderText("Select or type image name");
    imageLayout->addWidget(imageCombo_, 1);

    searchButton_ = new QPushButton("Search");
    connect(searchButton_, &QPushButton::clicked, this, &BasicInfoPage::searchImages);
    imageLayout->addWidget(searchButton_);

    layout->addRow("Image:", imageLayout);
    registerField("image*", imageCombo_);

    // Tag
    tagEdit_ = new QLineEdit("latest");
    layout->addRow("Tag:", tagEdit_);
    registerField("tag", tagEdit_);

    // Hostname
    hostnameEdit_ = new QLineEdit();
    hostnameEdit_->setPlaceholderText("Leave empty to use container name");
    layout->addRow("Hostname:", hostnameEdit_);
    registerField("hostname", hostnameEdit_);

    // Restart policy
    restartPolicyCombo_ = new QComboBox();
    restartPolicyCombo_->addItems({"no", "always", "unless-stopped", "on-failure"});
    layout->addRow("Restart Policy:", restartPolicyCombo_);
    registerField("restartPolicy", restartPolicyCombo_);
}

void BasicInfoPage::initializePage() {
    refreshImageList();
}

void BasicInfoPage::refreshImageList() {
    if (!dockerClient_) return;

    imageCombo_->clear();
    auto images = dockerClient_->listImages();

    for (const auto& image : images) {
        for (const auto& tag : image.repoTags) {
            imageCombo_->addItem(QString::fromStdString(tag));
        }
    }
}

void BasicInfoPage::searchImages() {
    // Placeholder for image search functionality
    QMessageBox::information(this, "Search Images",
        "Image search functionality will be available in a future update.\n"
        "For now, please type the image name directly.");
}

bool BasicInfoPage::validatePage() {
    if (nameEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Container name is required.");
        return false;
    }

    if (imageCombo_->currentText().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Image is required.");
        return false;
    }

    return true;
}

// ============================================================================
// NetworkConfigPage
// ============================================================================

NetworkConfigPage::NetworkConfigPage(std::shared_ptr<docker::DockerClient> dockerClient,
                                     QWidget* parent)
    : QWizardPage(parent)
    , dockerClient_(dockerClient)
{
    setTitle("Network Configuration");
    setSubTitle("Configure network settings and port mappings");

    auto* layout = new QVBoxLayout(this);

    // Port mappings
    auto* portsGroup = new QGroupBox("Port Mappings");
    auto* portsLayout = new QVBoxLayout(portsGroup);

    portsTable_ = new QTableWidget(0, 3);
    portsTable_->setHorizontalHeaderLabels({"Host Port", "Container Port", "Protocol"});
    portsTable_->horizontalHeader()->setStretchLastSection(true);
    portsLayout->addWidget(portsTable_);

    auto* portsButtons = new QHBoxLayout();
    auto* addPortBtn = new QPushButton("Add Port");
    connect(addPortBtn, &QPushButton::clicked, this, &NetworkConfigPage::addPortMapping);
    portsButtons->addWidget(addPortBtn);

    auto* removePortBtn = new QPushButton("Remove Selected");
    connect(removePortBtn, &QPushButton::clicked, this, &NetworkConfigPage::removePortMapping);
    portsButtons->addWidget(removePortBtn);
    portsButtons->addStretch();

    portsLayout->addLayout(portsButtons);
    layout->addWidget(portsGroup);

    // Networks
    auto* networksGroup = new QGroupBox("Networks");
    auto* networksLayout = new QVBoxLayout(networksGroup);

    auto* networkSelectLayout = new QHBoxLayout();
    availableNetworksCombo_ = new QComboBox();
    networkSelectLayout->addWidget(new QLabel("Network:"));
    networkSelectLayout->addWidget(availableNetworksCombo_, 1);

    auto* addNetworkBtn = new QPushButton("Add");
    connect(addNetworkBtn, &QPushButton::clicked, this, &NetworkConfigPage::addNetwork);
    networkSelectLayout->addWidget(addNetworkBtn);
    networksLayout->addLayout(networkSelectLayout);

    networksList_ = new QListWidget();
    networksLayout->addWidget(networksList_);

    auto* removeNetworkBtn = new QPushButton("Remove Selected");
    connect(removeNetworkBtn, &QPushButton::clicked, this, &NetworkConfigPage::removeNetwork);
    networksLayout->addWidget(removeNetworkBtn);

    layout->addWidget(networksGroup);

    // DNS
    auto* dnsGroup = new QGroupBox("DNS Servers");
    auto* dnsLayout = new QVBoxLayout(dnsGroup);

    dnsList_ = new QListWidget();
    dnsLayout->addWidget(dnsList_);

    auto* dnsButtons = new QHBoxLayout();
    auto* addDnsBtn = new QPushButton("Add DNS Server");
    connect(addDnsBtn, &QPushButton::clicked, this, &NetworkConfigPage::addDnsServer);
    dnsButtons->addWidget(addDnsBtn);

    auto* removeDnsBtn = new QPushButton("Remove Selected");
    connect(removeDnsBtn, &QPushButton::clicked, this, &NetworkConfigPage::removeDnsServer);
    dnsButtons->addWidget(removeDnsBtn);
    dnsButtons->addStretch();

    dnsLayout->addLayout(dnsButtons);
    layout->addWidget(dnsGroup);

    // Publish all ports
    publishAllPortsCheck_ = new QCheckBox("Publish all exposed ports to random ports");
    layout->addWidget(publishAllPortsCheck_);
}

void NetworkConfigPage::initializePage() {
    if (!dockerClient_) return;

    // Load available networks
    availableNetworksCombo_->clear();
    auto networks = dockerClient_->listNetworks();

    for (const auto& network : networks) {
        availableNetworksCombo_->addItem(QString::fromStdString(network.name));
    }
}

void NetworkConfigPage::addPortMapping() {
    int row = portsTable_->rowCount();
    portsTable_->insertRow(row);

    portsTable_->setItem(row, 0, new QTableWidgetItem("8080"));
    portsTable_->setItem(row, 1, new QTableWidgetItem("80"));

    auto* protocolCombo = new QComboBox();
    protocolCombo->addItems({"tcp", "udp"});
    portsTable_->setCellWidget(row, 2, protocolCombo);
}

void NetworkConfigPage::removePortMapping() {
    auto selected = portsTable_->selectedItems();
    if (!selected.isEmpty()) {
        portsTable_->removeRow(selected.first()->row());
    }
}

void NetworkConfigPage::addNetwork() {
    QString network = availableNetworksCombo_->currentText();
    if (!network.isEmpty()) {
        networksList_->addItem(network);
    }
}

void NetworkConfigPage::removeNetwork() {
    auto selected = networksList_->selectedItems();
    for (auto* item : selected) {
        delete item;
    }
}

void NetworkConfigPage::addDnsServer() {
    bool ok;
    QString dns = QInputDialog::getText(this, "Add DNS Server",
                                       "DNS Server IP:", QLineEdit::Normal,
                                       "8.8.8.8", &ok);
    if (ok && !dns.isEmpty()) {
        dnsList_->addItem(dns);
    }
}

void NetworkConfigPage::removeDnsServer() {
    auto selected = dnsList_->selectedItems();
    for (auto* item : selected) {
        delete item;
    }
}

// ============================================================================
// VolumeConfigPage
// ============================================================================

VolumeConfigPage::VolumeConfigPage(std::shared_ptr<docker::DockerClient> dockerClient,
                                   QWidget* parent)
    : QWizardPage(parent)
    , dockerClient_(dockerClient)
{
    setTitle("Volumes and Storage");
    setSubTitle("Configure volume mounts and bind mounts");

    auto* layout = new QVBoxLayout(this);

    // Volume table
    volumesTable_ = new QTableWidget(0, 4);
    volumesTable_->setHorizontalHeaderLabels({"Type", "Source", "Target", "Mode"});
    volumesTable_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(volumesTable_);

    // Add volume controls
    auto* addGroup = new QGroupBox("Add Volume Mount");
    auto* addLayout = new QFormLayout(addGroup);

    auto* typeCombo = new QComboBox();
    typeCombo->addItems({"Volume", "Bind Mount"});
    addLayout->addRow("Type:", typeCombo);

    auto* sourceLayout = new QHBoxLayout();
    availableVolumesCombo_ = new QComboBox();
    availableVolumesCombo_->setEditable(true);
    sourceLayout->addWidget(availableVolumesCombo_, 1);

    auto* browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &VolumeConfigPage::browseHostPath);
    sourceLayout->addWidget(browseBtn);
    addLayout->addRow("Source:", sourceLayout);

    containerPathEdit_ = new QLineEdit();
    containerPathEdit_->setPlaceholderText("/data");
    addLayout->addRow("Container Path:", containerPathEdit_);

    accessModeCombo_ = new QComboBox();
    accessModeCombo_->addItems({"rw", "ro"});
    addLayout->addRow("Access Mode:", accessModeCombo_);

    auto* addBtn = new QPushButton("Add Volume Mount");
    connect(addBtn, &QPushButton::clicked, this, &VolumeConfigPage::addVolumeMount);
    addLayout->addRow(addBtn);

    layout->addWidget(addGroup);

    // Remove button
    auto* removeBtn = new QPushButton("Remove Selected");
    connect(removeBtn, &QPushButton::clicked, this, &VolumeConfigPage::removeVolumeMount);
    layout->addWidget(removeBtn);
}

void VolumeConfigPage::initializePage() {
    if (!dockerClient_) return;

    // Load available volumes
    availableVolumesCombo_->clear();
    auto volumes = dockerClient_->listVolumes();

    for (const auto& volume : volumes) {
        availableVolumesCombo_->addItem(QString::fromStdString(volume.name));
    }
}

void VolumeConfigPage::addVolumeMount() {
    int row = volumesTable_->rowCount();
    volumesTable_->insertRow(row);

    volumesTable_->setItem(row, 0, new QTableWidgetItem("Volume"));
    volumesTable_->setItem(row, 1, new QTableWidgetItem(availableVolumesCombo_->currentText()));
    volumesTable_->setItem(row, 2, new QTableWidgetItem(containerPathEdit_->text()));
    volumesTable_->setItem(row, 3, new QTableWidgetItem(accessModeCombo_->currentText()));
}

void VolumeConfigPage::removeVolumeMount() {
    auto selected = volumesTable_->selectedItems();
    if (!selected.isEmpty()) {
        volumesTable_->removeRow(selected.first()->row());
    }
}

void VolumeConfigPage::browseHostPath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory");
    if (!dir.isEmpty()) {
        availableVolumesCombo_->setCurrentText(dir);
    }
}

// ============================================================================
// EnvironmentPage
// ============================================================================

EnvironmentPage::EnvironmentPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Environment Variables");
    setSubTitle("Set environment variables for the container");

    auto* layout = new QVBoxLayout(this);

    // Environment table
    envTable_ = new QTableWidget(0, 2);
    envTable_->setHorizontalHeaderLabels({"Variable Name", "Value"});
    envTable_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(envTable_);

    // Add variable controls
    auto* addLayout = new QHBoxLayout();
    keyEdit_ = new QLineEdit();
    keyEdit_->setPlaceholderText("KEY");
    addLayout->addWidget(new QLabel("Name:"));
    addLayout->addWidget(keyEdit_);

    valueEdit_ = new QLineEdit();
    valueEdit_->setPlaceholderText("value");
    addLayout->addWidget(new QLabel("Value:"));
    addLayout->addWidget(valueEdit_);

    auto* addBtn = new QPushButton("Add");
    connect(addBtn, &QPushButton::clicked, this, &EnvironmentPage::addVariable);
    addLayout->addWidget(addBtn);

    layout->addLayout(addLayout);

    // Action buttons
    auto* buttonsLayout = new QHBoxLayout();
    auto* removeBtn = new QPushButton("Remove Selected");
    connect(removeBtn, &QPushButton::clicked, this, &EnvironmentPage::removeVariable);
    buttonsLayout->addWidget(removeBtn);

    auto* loadBtn = new QPushButton("Load from .env file");
    connect(loadBtn, &QPushButton::clicked, this, &EnvironmentPage::loadFromFile);
    buttonsLayout->addWidget(loadBtn);
    buttonsLayout->addStretch();

    layout->addLayout(buttonsLayout);
}

void EnvironmentPage::addVariable() {
    if (keyEdit_->text().isEmpty()) return;

    int row = envTable_->rowCount();
    envTable_->insertRow(row);

    envTable_->setItem(row, 0, new QTableWidgetItem(keyEdit_->text()));
    envTable_->setItem(row, 1, new QTableWidgetItem(valueEdit_->text()));

    keyEdit_->clear();
    valueEdit_->clear();
}

void EnvironmentPage::removeVariable() {
    auto selected = envTable_->selectedItems();
    if (!selected.isEmpty()) {
        envTable_->removeRow(selected.first()->row());
    }
}

void EnvironmentPage::loadFromFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load Environment File",
                                                    "", "Environment Files (*.env);;All Files (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open file");
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        int eqPos = line.indexOf('=');
        if (eqPos > 0) {
            QString key = line.left(eqPos).trimmed();
            QString value = line.mid(eqPos + 1).trimmed();

            int row = envTable_->rowCount();
            envTable_->insertRow(row);
            envTable_->setItem(row, 0, new QTableWidgetItem(key));
            envTable_->setItem(row, 1, new QTableWidgetItem(value));
        }
    }
}

// ============================================================================
// ResourcesPage
// ============================================================================

ResourcesPage::ResourcesPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Resource Limits");
    setSubTitle("Configure CPU and memory limits");

    auto* layout = new QFormLayout(this);

    // CPU
    cpuSharesSpin_ = new QSpinBox();
    cpuSharesSpin_->setRange(0, 1024);
    cpuSharesSpin_->setValue(0);
    cpuSharesSpin_->setSpecialValueText("Unlimited");
    layout->addRow("CPU Shares:", cpuSharesSpin_);

    cpuQuotaSpin_ = new QDoubleSpinBox();
    cpuQuotaSpin_->setRange(0.01, 128.0);
    cpuQuotaSpin_->setValue(0);
    cpuQuotaSpin_->setSingleStep(0.1);
    cpuQuotaSpin_->setSpecialValueText("Unlimited");
    layout->addRow("CPU Quota:", cpuQuotaSpin_);

    // Memory
    auto* memoryLayout = new QHBoxLayout();
    memorySpin_ = new QSpinBox();
    memorySpin_->setRange(0, 999999);
    memorySpin_->setValue(0);
    memorySpin_->setSpecialValueText("Unlimited");
    memoryLayout->addWidget(memorySpin_);

    memoryUnitCombo_ = new QComboBox();
    memoryUnitCombo_->addItems({"MB", "GB"});
    memoryLayout->addWidget(memoryUnitCombo_);

    layout->addRow("Memory Limit:", memoryLayout);

    // Memory swap
    memorySwapSpin_ = new QSpinBox();
    memorySwapSpin_->setRange(-1, 999999);
    memorySwapSpin_->setValue(-1);
    memorySwapSpin_->setSpecialValueText("Same as memory");
    layout->addRow("Memory Swap:", memorySwapSpin_);

    // PID limit
    pidLimitSpin_ = new QSpinBox();
    pidLimitSpin_->setRange(0, 999999);
    pidLimitSpin_->setValue(0);
    pidLimitSpin_->setSpecialValueText("Unlimited");
    layout->addRow("PID Limit:", pidLimitSpin_);

    // OOM killer
    oomKillDisableCheck_ = new QCheckBox("Disable OOM Killer");
    layout->addRow("", oomKillDisableCheck_);
}

// ============================================================================
// AdvancedOptionsPage
// ============================================================================

AdvancedOptionsPage::AdvancedOptionsPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Advanced Options");
    setSubTitle("Configure advanced container options");

    auto* layout = new QVBoxLayout(this);

    // Basic options
    auto* basicGroup = new QGroupBox("Basic Options");
    auto* basicLayout = new QFormLayout(basicGroup);

    privilegedCheck_ = new QCheckBox("Run in privileged mode");
    basicLayout->addRow(privilegedCheck_);

    userEdit_ = new QLineEdit();
    userEdit_->setPlaceholderText("root");
    basicLayout->addRow("User:", userEdit_);

    workingDirEdit_ = new QLineEdit();
    workingDirEdit_->setPlaceholderText("/");
    basicLayout->addRow("Working Directory:", workingDirEdit_);

    layout->addWidget(basicGroup);

    // Command and Entrypoint
    auto* cmdGroup = new QGroupBox("Command Override");
    auto* cmdLayout = new QVBoxLayout(cmdGroup);

    cmdLayout->addWidget(new QLabel("Entrypoint (one per line):"));
    entrypointEdit_ = new QTextEdit();
    entrypointEdit_->setMaximumHeight(60);
    cmdLayout->addWidget(entrypointEdit_);

    cmdLayout->addWidget(new QLabel("Command (one argument per line):"));
    commandEdit_ = new QTextEdit();
    commandEdit_->setMaximumHeight(60);
    cmdLayout->addWidget(commandEdit_);

    layout->addWidget(cmdGroup);

    // Labels
    auto* labelsGroup = new QGroupBox("Labels");
    auto* labelsLayout = new QVBoxLayout(labelsGroup);

    labelsTable_ = new QTableWidget(0, 2);
    labelsTable_->setHorizontalHeaderLabels({"Key", "Value"});
    labelsTable_->horizontalHeader()->setStretchLastSection(true);
    labelsLayout->addWidget(labelsTable_);

    auto* labelButtons = new QHBoxLayout();
    auto* addLabelBtn = new QPushButton("Add Label");
    connect(addLabelBtn, &QPushButton::clicked, this, &AdvancedOptionsPage::addLabel);
    labelButtons->addWidget(addLabelBtn);

    auto* removeLabelBtn = new QPushButton("Remove Selected");
    connect(removeLabelBtn, &QPushButton::clicked, this, &AdvancedOptionsPage::removeLabel);
    labelButtons->addWidget(removeLabelBtn);
    labelButtons->addStretch();

    labelsLayout->addLayout(labelButtons);
    layout->addWidget(labelsGroup);
}

void AdvancedOptionsPage::addCapability() {
    QString cap = availableCapabilitiesCombo_->currentText();
    if (!cap.isEmpty()) {
        capAddList_->addItem(cap);
    }
}

void AdvancedOptionsPage::removeCapability() {
    auto selected = capAddList_->selectedItems();
    for (auto* item : selected) {
        delete item;
    }
}

void AdvancedOptionsPage::addLabel() {
    bool ok;
    QString key = QInputDialog::getText(this, "Add Label", "Label Key:", QLineEdit::Normal, "", &ok);
    if (!ok || key.isEmpty()) return;

    QString value = QInputDialog::getText(this, "Add Label", "Label Value:", QLineEdit::Normal, "", &ok);
    if (!ok) return;

    int row = labelsTable_->rowCount();
    labelsTable_->insertRow(row);
    labelsTable_->setItem(row, 0, new QTableWidgetItem(key));
    labelsTable_->setItem(row, 1, new QTableWidgetItem(value));
}

void AdvancedOptionsPage::removeLabel() {
    auto selected = labelsTable_->selectedItems();
    if (!selected.isEmpty()) {
        labelsTable_->removeRow(selected.first()->row());
    }
}

// ============================================================================
// HealthCheckPage
// ============================================================================

HealthCheckPage::HealthCheckPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Health Check");
    setSubTitle("Configure container health check");

    auto* layout = new QVBoxLayout(this);

    enableHealthCheckCheck_ = new QCheckBox("Enable health check");
    layout->addWidget(enableHealthCheckCheck_);

    auto* formLayout = new QFormLayout();

    testCommandEdit_ = new QLineEdit();
    testCommandEdit_->setPlaceholderText("curl -f http://localhost/ || exit 1");
    formLayout->addRow("Test Command:", testCommandEdit_);

    intervalSpin_ = new QSpinBox();
    intervalSpin_->setRange(1, 300);
    intervalSpin_->setValue(30);
    intervalSpin_->setSuffix(" seconds");
    formLayout->addRow("Interval:", intervalSpin_);

    timeoutSpin_ = new QSpinBox();
    timeoutSpin_->setRange(1, 300);
    timeoutSpin_->setValue(3);
    timeoutSpin_->setSuffix(" seconds");
    formLayout->addRow("Timeout:", timeoutSpin_);

    retriesSpin_ = new QSpinBox();
    retriesSpin_->setRange(1, 10);
    retriesSpin_->setValue(3);
    formLayout->addRow("Retries:", retriesSpin_);

    startPeriodSpin_ = new QSpinBox();
    startPeriodSpin_->setRange(0, 300);
    startPeriodSpin_->setValue(0);
    startPeriodSpin_->setSuffix(" seconds");
    formLayout->addRow("Start Period:", startPeriodSpin_);

    layout->addLayout(formLayout);
    layout->addStretch();
}

// ============================================================================
// SummaryPage
// ============================================================================

SummaryPage::SummaryPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Summary");
    setSubTitle("Review configuration before creating container");

    auto* layout = new QVBoxLayout(this);

    summaryText_ = new QTextEdit();
    summaryText_->setReadOnly(true);
    summaryText_->setFont(QFont("Courier New", 9));
    layout->addWidget(summaryText_);

    startAfterCreationCheck_ = new QCheckBox("Start container after creation");
    startAfterCreationCheck_->setChecked(true);
    layout->addWidget(startAfterCreationCheck_);
    registerField("startAfterCreation", startAfterCreationCheck_);
}

void SummaryPage::initializePage() {
    QString summary;
    summary += "Container Configuration Summary\n";
    summary += "==============================\n\n";

    summary += "Basic Information:\n";
    summary += "  Name: " + field("containerName").toString() + "\n";
    summary += "  Image: " + field("image").toString() + ":" + field("tag").toString() + "\n";
    summary += "  Hostname: " + field("hostname").toString() + "\n";
    summary += "  Restart Policy: " + field("restartPolicy").toString() + "\n\n";

    summary += "Network:\n";
    summary += "  (Configure ports, networks, and DNS in previous steps)\n\n";

    summary += "Volumes:\n";
    summary += "  (Configure volumes in previous steps)\n\n";

    summary += "Environment:\n";
    summary += "  (Configure environment variables in previous steps)\n\n";

    summaryText_->setPlainText(summary);
}

} // namespace ui
