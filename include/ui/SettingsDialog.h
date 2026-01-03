#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <memory>

namespace ui {

/**
 * SettingsDialog - Application settings configuration
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    // Settings structure
    struct AppSettings {
        // General
        bool autoRefresh = true;
        int refreshInterval = 30; // seconds
        bool confirmDestructiveActions = true;
        bool minimizeToTray = false;
        bool startMinimized = false;

        // Docker
        std::string dockerHost = "";  // Empty = use default
        int dockerTimeout = 30;
        bool useTLS = false;
        std::string tlsCertPath = "";
        std::string tlsKeyPath = "";
        std::string tlsCaPath = "";

        // Updates
        std::string updateStrategy = "Moderate";
        bool autoUpdate = false;
        bool checkUpdatesOnStartup = true;
        int updateCheckInterval = 24; // hours
        std::vector<std::string> excludedContainers;

        // Diagnostics
        bool autoRunDiagnostics = true;
        int diagnosticsInterval = 300; // seconds
        bool autoFix = false;
        bool notifyOnIssues = true;

        // UI
        std::string theme = "Light";
        bool showSystemTray = true;
        int logRetentionDays = 30;
        bool enableNotifications = true;

        // Advanced
        bool enableDebugLogging = false;
        int maxLogSize = 100; // MB
        std::string dataDirectory = "";
    };

    AppSettings getSettings() const;
    void setSettings(const AppSettings& settings);

signals:
    void settingsChanged(const AppSettings& newSettings);

private slots:
    void saveSettings();
    void resetToDefaults();
    void testDockerConnection();
    void browseDataDirectory();
    void browseTLSCertificate();
    void browseComposeDirectory();
    void addExcludedContainer();
    void removeExcludedContainer();

private:
    void setupUI();
    void loadSettings();
    void applySettings();
    void createGeneralTab();
    void createDockerTab();
    void createUpdatesTab();
    void createDiagnosticsTab();
    void createUITab();
    void createAdvancedTab();

    AppSettings settings_;
    QTabWidget* tabWidget_;

    // General tab
    QCheckBox* autoRefreshCheck_;
    QSpinBox* refreshIntervalSpin_;
    QCheckBox* confirmActionsCheck_;
    QCheckBox* minimizeToTrayCheck_;
    QCheckBox* startMinimizedCheck_;

    // Docker tab
    QLineEdit* dockerHostEdit_;
    QSpinBox* dockerTimeoutSpin_;
    QCheckBox* useTLSCheck_;
    QLineEdit* tlsCertEdit_;
    QLineEdit* tlsKeyEdit_;
    QLineEdit* tlsCaEdit_;
    QPushButton* testConnectionButton_;

    // Updates tab
    QComboBox* updateStrategyCombo_;
    QCheckBox* autoUpdateCheck_;
    QCheckBox* checkUpdatesOnStartupCheck_;
    QSpinBox* updateCheckIntervalSpin_;
    QListWidget* excludedContainersList_;
    QPushButton* addExcludedButton_;
    QPushButton* removeExcludedButton_;

    // Diagnostics tab
    QCheckBox* autoRunDiagnosticsCheck_;
    QSpinBox* diagnosticsIntervalSpin_;
    QCheckBox* autoFixCheck_;
    QCheckBox* notifyOnIssuesCheck_;

    // UI tab
    QComboBox* themeCombo_;
    QCheckBox* showSystemTrayCheck_;
    QSpinBox* logRetentionSpin_;
    QCheckBox* enableNotificationsCheck_;

    // Advanced tab
    QCheckBox* debugLoggingCheck_;
    QSpinBox* maxLogSizeSpin_;
    QLineEdit* dataDirectoryEdit_;
    QPushButton* browseDataDirButton_;
};

} // namespace ui
