#include "ui/LogsViewerDialog.h"
#include "docker/DockerClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>

namespace ui {

LogsViewerDialog::LogsViewerDialog(std::shared_ptr<docker::DockerClient> dockerClient,
                                   const std::string& containerId,
                                   const std::string& containerName,
                                   QWidget* parent)
    : QDialog(parent)
    , dockerClient_(dockerClient)
    , containerId_(containerId)
    , containerName_(containerName)
    , followMode_(false)
    , tailLines_(1000)
{
    setWindowTitle(QString("Container Logs - %1").arg(QString::fromStdString(containerName_)));
    resize(900, 600);

    setupUI();
    loadInitialLogs();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &LogsViewerDialog::refreshLogs);
}

LogsViewerDialog::~LogsViewerDialog() {
    if (refreshTimer_->isActive()) {
        refreshTimer_->stop();
    }
}

void LogsViewerDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Logs text area
    logsTextEdit_ = new QTextEdit();
    logsTextEdit_->setReadOnly(true);
    logsTextEdit_->setFont(QFont("Courier New", 9));
    logsTextEdit_->setLineWrapMode(QTextEdit::NoWrap);
    mainLayout->addWidget(logsTextEdit_);

    // Controls section
    auto* controlsLayout = new QHBoxLayout();

    // Left side controls
    refreshButton_ = new QPushButton("Refresh");
    connect(refreshButton_, &QPushButton::clicked, this, &LogsViewerDialog::refreshLogs);
    controlsLayout->addWidget(refreshButton_);

    clearButton_ = new QPushButton("Clear");
    connect(clearButton_, &QPushButton::clicked, this, &LogsViewerDialog::clearLogs);
    controlsLayout->addWidget(clearButton_);

    saveButton_ = new QPushButton("Save to File");
    connect(saveButton_, &QPushButton::clicked, this, &LogsViewerDialog::saveLogsToFile);
    controlsLayout->addWidget(saveButton_);

    copyButton_ = new QPushButton("Copy to Clipboard");
    connect(copyButton_, &QPushButton::clicked, this, &LogsViewerDialog::copyLogsToClipboard);
    controlsLayout->addWidget(copyButton_);

    searchButton_ = new QPushButton("Search");
    connect(searchButton_, &QPushButton::clicked, this, &LogsViewerDialog::searchLogs);
    controlsLayout->addWidget(searchButton_);

    controlsLayout->addStretch();

    mainLayout->addLayout(controlsLayout);

    // Options section
    auto* optionsGroup = new QGroupBox("Options");
    auto* optionsLayout = new QHBoxLayout(optionsGroup);

    // Auto-refresh checkbox
    autoRefreshCheckbox_ = new QCheckBox("Auto-refresh");
    connect(autoRefreshCheckbox_, &QCheckBox::toggled, this, &LogsViewerDialog::toggleAutoRefresh);
    optionsLayout->addWidget(autoRefreshCheckbox_);

    // Refresh interval
    optionsLayout->addWidget(new QLabel("Interval (s):"));
    refreshIntervalSpinBox_ = new QSpinBox();
    refreshIntervalSpinBox_->setRange(1, 60);
    refreshIntervalSpinBox_->setValue(5);
    connect(refreshIntervalSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LogsViewerDialog::updateRefreshInterval);
    optionsLayout->addWidget(refreshIntervalSpinBox_);

    // Follow mode
    followCheckbox_ = new QCheckBox("Follow (scroll to end)");
    connect(followCheckbox_, &QCheckBox::toggled, this, &LogsViewerDialog::toggleFollowMode);
    optionsLayout->addWidget(followCheckbox_);

    // Timestamps
    timestampsCheckbox_ = new QCheckBox("Show timestamps");
    timestampsCheckbox_->setChecked(true);
    connect(timestampsCheckbox_, &QCheckBox::toggled, this, &LogsViewerDialog::refreshLogs);
    optionsLayout->addWidget(timestampsCheckbox_);

    // Error only filter
    errorOnlyCheckbox_ = new QCheckBox("Errors only");
    connect(errorOnlyCheckbox_, &QCheckBox::toggled, this, &LogsViewerDialog::applyFilters);
    optionsLayout->addWidget(errorOnlyCheckbox_);

    // Tail lines
    optionsLayout->addWidget(new QLabel("Tail lines:"));
    tailLinesSpinBox_ = new QSpinBox();
    tailLinesSpinBox_->setRange(100, 10000);
    tailLinesSpinBox_->setSingleStep(100);
    tailLinesSpinBox_->setValue(tailLines_);
    connect(tailLinesSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int value) {
                tailLines_ = value;
                refreshLogs();
            });
    optionsLayout->addWidget(tailLinesSpinBox_);

    optionsLayout->addStretch();

    mainLayout->addWidget(optionsGroup);

    // Close button
    auto* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
}

void LogsViewerDialog::loadInitialLogs() {
    refreshLogs();
}

void LogsViewerDialog::refreshLogs() {
    if (!dockerClient_) {
        return;
    }

    std::string logs = dockerClient_->getContainerLogs(containerId_, tailLines_);

    if (logs.empty()) {
        logsTextEdit_->setPlainText("No logs available");
        return;
    }

    lastLogContent_ = logs;

    // Process timestamps option
    QString displayLogs = QString::fromStdString(logs);

    if (!timestampsCheckbox_->isChecked()) {
        // Remove timestamps (format: YYYY-MM-DDTHH:MM:SS.000000000Z)
        QStringList lines = displayLogs.split('\n');
        QStringList processedLines;

        for (const QString& line : lines) {
            QString processed = line;
            // Look for ISO 8601 timestamp at start
            if (line.length() > 30 && line[10] == 'T') {
                // Remove timestamp
                processed = line.mid(31); // Skip timestamp and space
            }
            processedLines.append(processed);
        }

        displayLogs = processedLines.join('\n');
    }

    logsTextEdit_->setPlainText(displayLogs);

    // Apply filters
    applyFilters();

    // Scroll to end if follow mode is enabled
    if (followMode_) {
        QTextCursor cursor = logsTextEdit_->textCursor();
        cursor.movePosition(QTextCursor::End);
        logsTextEdit_->setTextCursor(cursor);
    }
}

void LogsViewerDialog::toggleAutoRefresh(bool enabled) {
    if (enabled) {
        int interval = refreshIntervalSpinBox_->value() * 1000; // Convert to milliseconds
        refreshTimer_->start(interval);
    } else {
        refreshTimer_->stop();
    }
}

void LogsViewerDialog::toggleFollowMode(bool enabled) {
    followMode_ = enabled;
    if (enabled) {
        // Enable auto-refresh when follow mode is on
        autoRefreshCheckbox_->setChecked(true);
    }
}

void LogsViewerDialog::updateRefreshInterval(int seconds) {
    if (refreshTimer_->isActive()) {
        refreshTimer_->setInterval(seconds * 1000);
    }
}

void LogsViewerDialog::clearLogs() {
    logsTextEdit_->clear();
    lastLogContent_.clear();
}

void LogsViewerDialog::saveLogsToFile() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Logs",
        QString::fromStdString(containerName_) + "_logs.txt",
        "Text Files (*.txt);;Log Files (*.log);;All Files (*)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to save logs to file");
        return;
    }

    QTextStream out(&file);
    out << logsTextEdit_->toPlainText();
    file.close();

    QMessageBox::information(this, "Success", "Logs saved successfully");
}

void LogsViewerDialog::copyLogsToClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(logsTextEdit_->toPlainText());

    QMessageBox::information(this, "Success", "Logs copied to clipboard");
}

void LogsViewerDialog::searchLogs() {
    bool ok;
    QString searchTerm = QInputDialog::getText(
        this,
        "Search Logs",
        "Enter search term:",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || searchTerm.isEmpty()) {
        return;
    }

    // Find all occurrences
    QTextCursor cursor = logsTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::Start);
    logsTextEdit_->setTextCursor(cursor);

    // Create format for highlighting
    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(Qt::yellow);
    highlightFormat.setForeground(Qt::black);

    // Clear previous highlights
    QTextCursor clearCursor(logsTextEdit_->document());
    clearCursor.select(QTextCursor::Document);
    QTextCharFormat normalFormat;
    clearCursor.setCharFormat(normalFormat);

    // Find and highlight
    int count = 0;
    while (!logsTextEdit_->find(searchTerm)) {
        if (logsTextEdit_->textCursor().atEnd()) {
            break;
        }
    }

    // Move back to start and highlight all
    cursor.movePosition(QTextCursor::Start);
    logsTextEdit_->setTextCursor(cursor);

    while (logsTextEdit_->find(searchTerm)) {
        QTextCursor highlightCursor = logsTextEdit_->textCursor();
        highlightCursor.mergeCharFormat(highlightFormat);
        count++;
    }

    // Move to first occurrence
    cursor.movePosition(QTextCursor::Start);
    logsTextEdit_->setTextCursor(cursor);
    logsTextEdit_->find(searchTerm);

    if (count > 0) {
        QMessageBox::information(this, "Search Results",
                               QString("Found %1 occurrence(s)").arg(count));
    } else {
        QMessageBox::information(this, "Search Results", "No matches found");
    }
}

void LogsViewerDialog::applyFilters() {
    if (!errorOnlyCheckbox_->isChecked()) {
        return;
    }

    // Filter to show only error lines
    QString allLogs = logsTextEdit_->toPlainText();
    QStringList lines = allLogs.split('\n');
    QStringList errorLines;

    for (const QString& line : lines) {
        QString lowerLine = line.toLower();
        if (lowerLine.contains("error") ||
            lowerLine.contains("err:") ||
            lowerLine.contains("fatal") ||
            lowerLine.contains("exception") ||
            lowerLine.contains("fail")) {
            errorLines.append(line);
        }
    }

    if (!errorLines.isEmpty()) {
        logsTextEdit_->setPlainText(errorLines.join('\n'));
    } else {
        logsTextEdit_->setPlainText("No error lines found");
    }
}

} // namespace ui
