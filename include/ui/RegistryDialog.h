#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <memory>

namespace registry {
    class RegistryManager;
    struct RegistryConfig;
}

namespace ui {

/**
 * RegistryDialog - Manage container registries
 */
class RegistryDialog : public QDialog {
    Q_OBJECT

public:
    explicit RegistryDialog(std::shared_ptr<registry::RegistryManager> registryMgr,
                           QWidget* parent = nullptr);
    ~RegistryDialog();

private slots:
    void addRegistry();
    void removeRegistry();
    void editRegistry();
    void testConnection();
    void refreshRegistryList();
    void onRegistrySelected();

private:
    void setupUI();
    void loadRegistries();

    std::shared_ptr<registry::RegistryManager> registryMgr_;

    QListWidget* registryList_;
    QPushButton* addButton_;
    QPushButton* removeButton_;
    QPushButton* editButton_;
    QPushButton* testButton_;
    QTextEdit* detailsText_;
};

/**
 * RegistryEditDialog - Add/edit registry configuration
 */
class RegistryEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit RegistryEditDialog(const registry::RegistryConfig* config = nullptr,
                               QWidget* parent = nullptr);
    ~RegistryEditDialog();

    registry::RegistryConfig getConfig() const;

private slots:
    void onTypeChanged(int index);
    void onAuthMethodChanged(int index);
    void browseFile();

private:
    void setupUI();
    void loadConfig(const registry::RegistryConfig& config);

    QLineEdit* nameEdit_;
    QComboBox* typeCombo_;
    QLineEdit* urlEdit_;
    QLineEdit* apiUrlEdit_;
    QCheckBox* secureCheck_;

    // Authentication
    QComboBox* authMethodCombo_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QLineEdit* tokenEdit_;
    QPushButton* browseTokenButton_;

    // Custom headers
    QTextEdit* headersEdit_;
};

} // namespace ui
