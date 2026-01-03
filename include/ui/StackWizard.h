#pragma once

#include <QWizard>
#include <QWizardPage>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <memory>

namespace compose {
    class ComposeStack;
    struct ComposeFile;
}

namespace docker {
    class DockerClient;
}

namespace ui {

/**
 * StackWizard - Wizard for creating Docker Compose stacks
 */
class StackWizard : public QWizard {
    Q_OBJECT

public:
    explicit StackWizard(std::shared_ptr<docker::DockerClient> dockerClient,
                        std::shared_ptr<compose::ComposeStack> stackManager,
                        QWidget* parent = nullptr);
    ~StackWizard();

    std::string getStackName() const;
    std::string getComposeContent() const;

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    std::shared_ptr<compose::ComposeStack> stackManager_;
};

// Page 1: Stack Information
class StackInfoPage : public QWizardPage {
    Q_OBJECT

public:
    explicit StackInfoPage(QWidget* parent = nullptr);

    bool validatePage() override;

private slots:
    void loadExistingCompose();

private:
    QLineEdit* stackNameEdit_;
    QLineEdit* projectNameEdit_;
    QComboBox* versionCombo_;
    QPushButton* loadButton_;
};

// Page 2: Services Definition
class ServicesPage : public QWizardPage {
    Q_OBJECT

public:
    explicit ServicesPage(std::shared_ptr<docker::DockerClient> dockerClient,
                         QWidget* parent = nullptr);

private slots:
    void addService();
    void removeService();
    void editService();
    void addServiceDependency();

private:
    std::shared_ptr<docker::DockerClient> dockerClient_;
    QListWidget* servicesList_;
    QTableWidget* servicesTable_;
    QPushButton* addButton_;
    QPushButton* removeButton_;
    QPushButton* editButton_;
};

// Page 3: Networks
class StackNetworksPage : public QWizardPage {
    Q_OBJECT

public:
    explicit StackNetworksPage(QWidget* parent = nullptr);

private slots:
    void addNetwork();
    void removeNetwork();
    void editNetwork();

private:
    QTableWidget* networksTable_;
};

// Page 4: Volumes
class StackVolumesPage : public QWizardPage {
    Q_OBJECT

public:
    explicit StackVolumesPage(QWidget* parent = nullptr);

private slots:
    void addVolume();
    void removeVolume();
    void editVolume();

private:
    QTableWidget* volumesTable_;
};

// Page 5: Generated YAML Review
class ComposeReviewPage : public QWizardPage {
    Q_OBJECT

public:
    explicit ComposeReviewPage(QWidget* parent = nullptr);

    void initializePage() override;

private slots:
    void saveToFile();
    void validateCompose();

private:
    QTextEdit* yamlEdit_;
    QPushButton* saveButton_;
    QPushButton* validateButton_;
    QLabel* validationLabel_;
};

// Page 6: Deployment Options
class DeploymentPage : public QWizardPage {
    Q_OBJECT

public:
    explicit DeploymentPage(QWidget* parent = nullptr);

private:
    QCheckBox* deployImmediatelyCheck_;
    QCheckBox* pullImagesCheck_;
    QCheckBox* recreateCheck_;
    QCheckBox* startServicesCheck_;
};

} // namespace ui
