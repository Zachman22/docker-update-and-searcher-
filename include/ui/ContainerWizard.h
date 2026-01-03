#pragma once

#include <QWizard>
#include <QWizardPage>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QListWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <memory>
#include <nlohmann/json.hpp>

namespace docker {
    class DockerClient;
}

namespace ui {

/**
 * ContainerWizard - Step-by-step container creation wizard
 */
class ContainerWizard : public QWizard {
    Q_OBJECT

public:
    explicit ContainerWizard(std::shared_ptr<docker::DockerClient> dockerClient,
                            QWidget* parent = nullptr);
    ~ContainerWizard();

    nlohmann::json getContainerConfig() const;
    std::string getContainerName() const;

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
};

// Page 1: Basic Information
class BasicInfoPage : public QWizardPage {
    Q_OBJECT

public:
    explicit BasicInfoPage(std::shared_ptr<docker::DockerClient> dockerClient,
                          QWidget* parent = nullptr);

    bool validatePage() override;
    void initializePage() override;

private slots:
    void searchImages();
    void refreshImageList();

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    QLineEdit* nameEdit_;
    QComboBox* imageCombo_;
    QLineEdit* tagEdit_;
    QPushButton* searchButton_;
    QLineEdit* hostnameEdit_;
    QComboBox* restartPolicyCombo_;
};

// Page 2: Network Configuration
class NetworkConfigPage : public QWizardPage {
    Q_OBJECT

public:
    explicit NetworkConfigPage(std::shared_ptr<docker::DockerClient> dockerClient,
                              QWidget* parent = nullptr);

    void initializePage() override;

private slots:
    void addPortMapping();
    void removePortMapping();
    void addNetwork();
    void removeNetwork();
    void addDnsServer();
    void removeDnsServer();

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    QTableWidget* portsTable_;
    QListWidget* networksList_;
    QComboBox* availableNetworksCombo_;
    QListWidget* dnsList_;
    QLineEdit* hostnameEdit_;
    QCheckBox* publishAllPortsCheck_;
};

// Page 3: Volumes and Storage
class VolumeConfigPage : public QWizardPage {
    Q_OBJECT

public:
    explicit VolumeConfigPage(std::shared_ptr<docker::DockerClient> dockerClient,
                             QWidget* parent = nullptr);

    void initializePage() override;

private slots:
    void addVolumeMount();
    void removeVolumeMount();
    void browseHostPath();

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    QTableWidget* volumesTable_;
    QComboBox* availableVolumesCombo_;
    QLineEdit* hostPathEdit_;
    QLineEdit* containerPathEdit_;
    QComboBox* accessModeCombo_;
};

// Page 4: Environment Variables
class EnvironmentPage : public QWizardPage {
    Q_OBJECT

public:
    explicit EnvironmentPage(QWidget* parent = nullptr);

private slots:
    void addVariable();
    void removeVariable();
    void loadFromFile();

private:
    QTableWidget* envTable_;
    QLineEdit* keyEdit_;
    QLineEdit* valueEdit_;
};

// Page 5: Resources and Limits
class ResourcesPage : public QWizardPage {
    Q_OBJECT

public:
    explicit ResourcesPage(QWidget* parent = nullptr);

private:
    QSpinBox* cpuSharesSpin_;
    QDoubleSpinBox* cpuQuotaSpin_;
    QSpinBox* memorySpin_;
    QComboBox* memoryUnitCombo_;
    QSpinBox* memorySwapSpin_;
    QSpinBox* pidLimitSpin_;
    QCheckBox* oomKillDisableCheck_;
};

// Page 6: Advanced Options
class AdvancedOptionsPage : public QWizardPage {
    Q_OBJECT

public:
    explicit AdvancedOptionsPage(QWidget* parent = nullptr);

private slots:
    void addCapability();
    void removeCapability();
    void addLabel();
    void removeLabel();

private:
    QCheckBox* privilegedCheck_;
    QLineEdit* userEdit_;
    QLineEdit* workingDirEdit_;
    QListWidget* capAddList_;
    QListWidget* capDropList_;
    QComboBox* availableCapabilitiesCombo_;
    QTableWidget* labelsTable_;
    QTextEdit* commandEdit_;
    QTextEdit* entrypointEdit_;
};

// Page 7: Health Check
class HealthCheckPage : public QWizardPage {
    Q_OBJECT

public:
    explicit HealthCheckPage(QWidget* parent = nullptr);

private:
    QCheckBox* enableHealthCheckCheck_;
    QLineEdit* testCommandEdit_;
    QSpinBox* intervalSpin_;
    QSpinBox* timeoutSpin_;
    QSpinBox* retriesSpin_;
    QSpinBox* startPeriodSpin_;
};

// Page 8: Summary and Review
class SummaryPage : public QWizardPage {
    Q_OBJECT

public:
    explicit SummaryPage(QWidget* parent = nullptr);

    void initializePage() override;

private:
    QTextEdit* summaryText_;
    QCheckBox* startAfterCreationCheck_;
};

} // namespace ui
