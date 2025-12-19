#include "storage/Database.h"
#include "utils/Logger.h"
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <ctime>

using json = nlohmann::json;

namespace storage {

// Pimpl for SQLite implementation
class Database::Impl {
public:
    sqlite3* db = nullptr;

    bool exec(const std::string& sql) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            std::string error = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            LOG_ERROR("SQL error: " + error);
            return false;
        }

        return true;
    }

    std::string getCurrentTimestamp() {
        auto now = std::time(nullptr);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buf);
    }
};

Database::Database(const std::string& dbPath)
    : pImpl_(std::make_unique<Impl>())
    , dbPath_(dbPath)
    , isOpen_(false)
{
    LOG_INFO("Database created: " + dbPath);
}

Database::~Database() {
    close();
}

bool Database::initialize() {
    LOG_INFO("Initializing database at: " + dbPath_);

    // Open database
    int rc = sqlite3_open(dbPath_.c_str(), &pImpl_->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open database: " + std::string(sqlite3_errmsg(pImpl_->db)));
        return false;
    }

    // Enable foreign keys
    pImpl_->exec("PRAGMA foreign_keys = ON;");

    // Create tables
    if (!createTables()) {
        LOG_ERROR("Failed to create tables");
        return false;
    }

    isOpen_ = true;
    LOG_INFO("Database initialized successfully");
    return true;
}

bool Database::migrate() {
    int currentVersion = getCurrentSchemaVersion();
    const int targetVersion = 1;

    if (currentVersion < targetVersion) {
        LOG_INFO("Migrating database schema from version " + std::to_string(currentVersion) +
                 " to " + std::to_string(targetVersion));
        return upgradeSchema(currentVersion, targetVersion);
    }

    return true;
}

bool Database::close() {
    if (isOpen_ && pImpl_->db) {
        LOG_INFO("Closing database");
        sqlite3_close(pImpl_->db);
        pImpl_->db = nullptr;
        isOpen_ = false;
    }
    return true;
}

bool Database::isOpen() const {
    return isOpen_;
}

bool Database::saveContainer(const docker::Container& container) {
    if (!isOpen_) return false;

    std::string json = serializeContainer(container);
    std::string sql = "INSERT OR REPLACE INTO containers (id, name, image, config, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement");
        return false;
    }

    std::string timestamp = pImpl_->getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, container.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, container.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, container.image.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to save container");
        return false;
    }

    LOG_INFO("Container saved: " + container.name);
    return true;
}

bool Database::updateContainer(const docker::Container& container) {
    return saveContainer(container);  // Using REPLACE INTO
}

std::optional<docker::Container> Database::getContainer(const std::string& id) {
    if (!isOpen_) return std::nullopt;

    std::string sql = "SELECT config FROM containers WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* configStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string config = configStr ? configStr : "{}";
        sqlite3_finalize(stmt);

        return deserializeContainer(config);
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<docker::Container> Database::getAllContainers() {
    std::vector<docker::Container> containers;
    if (!isOpen_) return containers;

    std::string sql = "SELECT config FROM containers ORDER BY name;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return containers;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* configStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (configStr) {
            containers.push_back(deserializeContainer(configStr));
        }
    }

    sqlite3_finalize(stmt);
    return containers;
}

bool Database::deleteContainer(const std::string& id) {
    if (!isOpen_) return false;

    std::string sql = "DELETE FROM containers WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::recordContainerAction(const ContainerHistory& history) {
    if (!isOpen_) return false;

    std::string sql = "INSERT INTO container_history (container_id, action, timestamp, details, performed_by) "
                      "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, history.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, history.action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, history.timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, history.details.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, history.performedBy.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<ContainerHistory> Database::getContainerHistory(const std::string& containerId, int limit) {
    std::vector<ContainerHistory> history;
    if (!isOpen_) return history;

    std::string sql = "SELECT container_id, action, timestamp, details, performed_by "
                      "FROM container_history WHERE container_id = ? "
                      "ORDER BY timestamp DESC LIMIT ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return history;
    }

    sqlite3_bind_text(stmt, 1, containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContainerHistory h;
        h.containerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        h.action = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        h.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        h.details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        h.performedBy = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        history.push_back(h);
    }

    sqlite3_finalize(stmt);
    return history;
}

std::vector<ContainerHistory> Database::getRecentHistory(int limit) {
    std::vector<ContainerHistory> history;
    if (!isOpen_) return history;

    std::string sql = "SELECT container_id, action, timestamp, details, performed_by "
                      "FROM container_history ORDER BY timestamp DESC LIMIT ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return history;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContainerHistory h;
        h.containerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        h.action = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        h.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        h.details = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        h.performedBy = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        history.push_back(h);
    }

    sqlite3_finalize(stmt);
    return history;
}

bool Database::recordUpdate(const UpdateHistory& update) {
    if (!isOpen_) return false;

    std::string sql = "INSERT INTO update_history (container_id, from_version, to_version, timestamp, success, error_message, rolled_back) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, update.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, update.fromVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, update.toVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, update.timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, update.success ? 1 : 0);
    sqlite3_bind_text(stmt, 6, update.errorMessage.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, update.rolledBack ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<UpdateHistory> Database::getUpdateHistory(const std::string& containerId) {
    std::vector<UpdateHistory> history;
    if (!isOpen_) return history;

    std::string sql = "SELECT container_id, from_version, to_version, timestamp, success, error_message, rolled_back "
                      "FROM update_history WHERE container_id = ? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return history;
    }

    sqlite3_bind_text(stmt, 1, containerId.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UpdateHistory h;
        h.containerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        h.fromVersion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        h.toVersion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        h.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        h.success = sqlite3_column_int(stmt, 4) != 0;
        h.errorMessage = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        h.rolledBack = sqlite3_column_int(stmt, 6) != 0;
        history.push_back(h);
    }

    sqlite3_finalize(stmt);
    return history;
}

std::optional<UpdateHistory> Database::getLastUpdate(const std::string& containerId) {
    auto history = getUpdateHistory(containerId);
    if (history.empty()) {
        return std::nullopt;
    }
    return history[0];
}

std::vector<UpdateHistory> Database::getFailedUpdates() {
    std::vector<UpdateHistory> history;
    if (!isOpen_) return history;

    std::string sql = "SELECT container_id, from_version, to_version, timestamp, success, error_message, rolled_back "
                      "FROM update_history WHERE success = 0 ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return history;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UpdateHistory h;
        h.containerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        h.fromVersion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        h.toVersion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        h.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        h.success = sqlite3_column_int(stmt, 4) != 0;
        h.errorMessage = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        h.rolledBack = sqlite3_column_int(stmt, 6) != 0;
        history.push_back(h);
    }

    sqlite3_finalize(stmt);
    return history;
}

bool Database::saveIssue(const diagnostics::DiagnosticIssue& issue) {
    if (!isOpen_) return false;

    std::string sql = "INSERT OR REPLACE INTO issues (id, container_id, category, severity, title, description, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    std::string timestamp = pImpl_->getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, issue.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, issue.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, issue.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, issue.severity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, issue.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, issue.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, timestamp.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::updateIssue(const diagnostics::DiagnosticIssue& issue) {
    return saveIssue(issue);  // Using REPLACE INTO
}

std::vector<diagnostics::DiagnosticIssue> Database::getActiveIssues() {
    std::vector<diagnostics::DiagnosticIssue> issues;
    if (!isOpen_) return issues;

    std::string sql = "SELECT id, container_id, category, severity, title, description "
                      "FROM issues WHERE resolved_at IS NULL ORDER BY created_at DESC;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return issues;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        diagnostics::DiagnosticIssue issue;
        issue.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        issue.containerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        issue.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        issue.severity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        issue.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        issue.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        issues.push_back(issue);
    }

    sqlite3_finalize(stmt);
    return issues;
}

std::vector<diagnostics::DiagnosticIssue> Database::getResolvedIssues(int limit) {
    std::vector<diagnostics::DiagnosticIssue> issues;
    if (!isOpen_) return issues;

    std::string sql = "SELECT id, container_id, category, severity, title, description "
                      "FROM issues WHERE resolved_at IS NOT NULL ORDER BY resolved_at DESC LIMIT ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return issues;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        diagnostics::DiagnosticIssue issue;
        issue.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        issue.containerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        issue.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        issue.severity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        issue.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        issue.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        issues.push_back(issue);
    }

    sqlite3_finalize(stmt);
    return issues;
}

bool Database::markIssueResolved(const std::string& issueId) {
    if (!isOpen_) return false;

    std::string sql = "UPDATE issues SET resolved_at = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    std::string timestamp = pImpl_->getCurrentTimestamp();
    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, issueId.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::saveSettings(const AppSettings& settings) {
    if (!isOpen_) return false;

    pImpl_->exec("BEGIN TRANSACTION;");

    updateSetting("autoUpdate", settings.autoUpdate ? "1" : "0");
    updateSetting("updateSchedule", settings.updateSchedule);
    updateSetting("updateStrategy", settings.updateStrategy);
    updateSetting("notificationsEnabled", settings.notificationsEnabled ? "1" : "0");
    updateSetting("dockerHost", settings.dockerHost);
    updateSetting("portScanRange", std::to_string(settings.portScanRange));
    updateSetting("enableHealthMonitoring", settings.enableHealthMonitoring ? "1" : "0");
    updateSetting("healthCheckInterval", std::to_string(settings.healthCheckInterval));

    pImpl_->exec("COMMIT;");
    return true;
}

std::optional<AppSettings> Database::getSettings() {
    if (!isOpen_) return std::nullopt;

    AppSettings settings;

    // TODO: Implement proper settings retrieval
    settings.autoUpdate = false;
    settings.updateSchedule = "";
    settings.updateStrategy = "moderate";
    settings.notificationsEnabled = true;
    settings.dockerHost = "unix:///var/run/docker.sock";
    settings.portScanRange = 65535;
    settings.enableHealthMonitoring = true;
    settings.healthCheckInterval = 60;

    return settings;
}

bool Database::updateSetting(const std::string& key, const std::string& value) {
    if (!isOpen_) return false;

    std::string sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

int Database::getTotalContainers() {
    if (!isOpen_) return 0;

    std::string sql = "SELECT COUNT(*) FROM containers;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int Database::getRunningContainers() {
    // TODO: Track container state in database
    return 0;
}

int Database::getPendingUpdates() {
    // TODO: Track pending updates
    return 0;
}

int Database::getActiveIssuesCount() {
    if (!isOpen_) return 0;

    std::string sql = "SELECT COUNT(*) FROM issues WHERE resolved_at IS NULL;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

std::map<std::string, int> Database::getIssuesByCategoryCount() {
    std::map<std::string, int> counts;
    if (!isOpen_) return counts;

    std::string sql = "SELECT category, COUNT(*) FROM issues WHERE resolved_at IS NULL GROUP BY category;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return counts;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int count = sqlite3_column_int(stmt, 1);
        counts[category] = count;
    }

    sqlite3_finalize(stmt);
    return counts;
}

bool Database::cleanupOldHistory(int daysToKeep) {
    if (!isOpen_) return false;

    std::string sql = "DELETE FROM container_history WHERE "
                      "julianday('now') - julianday(timestamp) > ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, daysToKeep);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    LOG_INFO("Cleaned up old history (> " + std::to_string(daysToKeep) + " days)");
    return rc == SQLITE_DONE;
}

bool Database::cleanupResolvedIssues(int daysToKeep) {
    if (!isOpen_) return false;

    std::string sql = "DELETE FROM issues WHERE resolved_at IS NOT NULL AND "
                      "julianday('now') - julianday(resolved_at) > ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, daysToKeep);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    LOG_INFO("Cleaned up resolved issues (> " + std::to_string(daysToKeep) + " days)");
    return rc == SQLITE_DONE;
}

bool Database::vacuum() {
    if (!isOpen_) return false;
    LOG_INFO("Vacuuming database");
    return pImpl_->exec("VACUUM;");
}

bool Database::createTables() {
    LOG_INFO("Creating database tables");

    if (!pImpl_->db) {
        LOG_ERROR("Database not open");
        return false;
    }

    // Containers table
    std::string containersSql = R"(
        CREATE TABLE IF NOT EXISTS containers (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            image TEXT NOT NULL,
            config TEXT,
            created_at DATETIME,
            updated_at DATETIME
        );
    )";

    // Container history table
    std::string historySql = R"(
        CREATE TABLE IF NOT EXISTS container_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            container_id TEXT NOT NULL,
            action TEXT NOT NULL,
            timestamp DATETIME NOT NULL,
            details TEXT,
            performed_by TEXT,
            FOREIGN KEY (container_id) REFERENCES containers(id) ON DELETE CASCADE
        );
    )";

    // Update history table
    std::string updateHistorySql = R"(
        CREATE TABLE IF NOT EXISTS update_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            container_id TEXT NOT NULL,
            from_version TEXT,
            to_version TEXT,
            timestamp DATETIME NOT NULL,
            success BOOLEAN NOT NULL,
            error_message TEXT,
            rolled_back BOOLEAN DEFAULT 0,
            FOREIGN KEY (container_id) REFERENCES containers(id) ON DELETE CASCADE
        );
    )";

    // Issues table
    std::string issuesSql = R"(
        CREATE TABLE IF NOT EXISTS issues (
            id TEXT PRIMARY KEY,
            container_id TEXT,
            category TEXT NOT NULL,
            severity TEXT NOT NULL,
            title TEXT NOT NULL,
            description TEXT,
            created_at DATETIME NOT NULL,
            resolved_at DATETIME,
            FOREIGN KEY (container_id) REFERENCES containers(id) ON DELETE CASCADE
        );
    )";

    // Settings table
    std::string settingsSql = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT
        );
    )";

    // Create indexes for performance
    std::string indexesSql = R"(
        CREATE INDEX IF NOT EXISTS idx_history_container ON container_history(container_id);
        CREATE INDEX IF NOT EXISTS idx_history_timestamp ON container_history(timestamp);
        CREATE INDEX IF NOT EXISTS idx_update_container ON update_history(container_id);
        CREATE INDEX IF NOT EXISTS idx_issues_container ON issues(container_id);
        CREATE INDEX IF NOT EXISTS idx_issues_resolved ON issues(resolved_at);
    )";

    // Execute all table creation statements
    if (!pImpl_->exec(containersSql)) return false;
    if (!pImpl_->exec(historySql)) return false;
    if (!pImpl_->exec(updateHistorySql)) return false;
    if (!pImpl_->exec(issuesSql)) return false;
    if (!pImpl_->exec(settingsSql)) return false;
    if (!pImpl_->exec(indexesSql)) return false;

    // Set schema version
    setSchemaVersion(1);

    LOG_INFO("Database tables created successfully");
    return true;
}

bool Database::upgradeSchema(int fromVersion, int toVersion) {
    LOG_INFO("Upgrading schema from " + std::to_string(fromVersion) +
             " to " + std::to_string(toVersion));

    // Future schema upgrades will be implemented here
    return true;
}

int Database::getCurrentSchemaVersion() {
    if (!isOpen_) return 0;

    std::string sql = "SELECT value FROM settings WHERE key = 'schema_version';";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* versionStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (versionStr) {
            version = std::stoi(versionStr);
        }
    }

    sqlite3_finalize(stmt);
    return version;
}

bool Database::setSchemaVersion(int version) {
    return updateSetting("schema_version", std::to_string(version));
}

std::string Database::serializeContainer(const docker::Container& container) {
    json j;
    j["id"] = container.id;
    j["name"] = container.name;
    j["image"] = container.image;
    j["state"] = static_cast<int>(container.state);
    j["created"] = container.created;

    // Serialize ports
    json portsArray = json::array();
    for (const auto& port : container.ports) {
        json portJson;
        portJson["container"] = port.containerPort;
        portJson["host"] = port.hostPort;
        portJson["protocol"] = port.protocol;
        portsArray.push_back(portJson);
    }
    j["ports"] = portsArray;

    // Serialize environment variables
    json envArray = json::array();
    for (const auto& [key, value] : container.environment) {
        json envJson;
        envJson["key"] = key;
        envJson["value"] = value;
        envArray.push_back(envJson);
    }
    j["environment"] = envArray;

    // Serialize volumes
    json volumesArray = json::array();
    for (const auto& vol : container.volumes) {
        json volJson;
        volJson["host"] = vol.hostPath;
        volJson["container"] = vol.containerPath;
        volJson["mode"] = vol.mode;
        volumesArray.push_back(volJson);
    }
    j["volumes"] = volumesArray;

    j["labels"] = container.labels;
    j["networks"] = container.networks;

    return j.dump();
}

docker::Container Database::deserializeContainer(const std::string& data) {
    docker::Container container;

    try {
        auto j = json::parse(data);

        container.id = j.value("id", "");
        container.name = j.value("name", "");
        container.image = j.value("image", "");
        container.state = static_cast<docker::ContainerState>(j.value("state", 0));
        container.created = j.value("created", "");

        // Deserialize ports
        if (j.contains("ports")) {
            for (const auto& portJson : j["ports"]) {
                docker::PortMapping port;
                port.containerPort = portJson.value("container", 0);
                port.hostPort = portJson.value("host", 0);
                port.protocol = portJson.value("protocol", "tcp");
                container.ports.push_back(port);
            }
        }

        // Deserialize environment
        if (j.contains("environment")) {
            for (const auto& envJson : j["environment"]) {
                container.environment[envJson["key"]] = envJson["value"];
            }
        }

        // Deserialize volumes
        if (j.contains("volumes")) {
            for (const auto& volJson : j["volumes"]) {
                docker::VolumeMount vol;
                vol.hostPath = volJson.value("host", "");
                vol.containerPath = volJson.value("container", "");
                vol.mode = volJson.value("mode", "rw");
                container.volumes.push_back(vol);
            }
        }

        if (j.contains("labels")) {
            container.labels = j["labels"].get<std::map<std::string, std::string>>();
        }

        if (j.contains("networks")) {
            container.networks = j["networks"].get<std::vector<std::string>>();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error deserializing container: " + std::string(e.what()));
    }

    return container;
}

} // namespace storage
