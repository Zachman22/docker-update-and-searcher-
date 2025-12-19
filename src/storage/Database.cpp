#include "storage/Database.h"
#include "utils/Logger.h"
#include <sqlite3.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace storage {

class Database::Impl {
public:
    sqlite3* db = nullptr;

    ~Impl() {
        if (db) {
            sqlite3_close(db);
        }
    }

    bool execute(const std::string& sql) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            LOG_ERROR("SQL error: " + std::string(errMsg));
            sqlite3_free(errMsg);
            return false;
        }
        return true;
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
    LOG_INFO("Initializing database");

    int rc = sqlite3_open(dbPath_.c_str(), &pImpl_->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open database: " + std::string(sqlite3_errmsg(pImpl_->db)));
        return false;
    }

    isOpen_ = true;

    if (!createTables()) {
        LOG_ERROR("Failed to create tables");
        return false;
    }

    LOG_INFO("Database initialized successfully");
    return true;
}

bool Database::migrate() {
    return true;
}

bool Database::close() {
    if (isOpen_ && pImpl_->db) {
        sqlite3_close(pImpl_->db);
        pImpl_->db = nullptr;
        isOpen_ = false;
        LOG_INFO("Database closed");
    }
    return true;
}

bool Database::isOpen() const {
    return isOpen_;
}

bool Database::createTables() {
    LOG_INFO("Creating database tables");

    // Containers table
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS containers (
            id TEXT PRIMARY KEY,
            name TEXT,
            image TEXT,
            tag TEXT,
            state TEXT,
            status TEXT,
            config TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    if (!pImpl_->execute(sql)) return false;

    // History table
    sql = R"(
        CREATE TABLE IF NOT EXISTS container_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            container_id TEXT,
            container_name TEXT,
            action TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            details TEXT,
            performed_by TEXT
        );
    )";

    if (!pImpl_->execute(sql)) return false;

    // Update history
    sql = R"(
        CREATE TABLE IF NOT EXISTS update_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            container_id TEXT,
            container_name TEXT,
            from_version TEXT,
            to_version TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            success BOOLEAN,
            error_message TEXT,
            rolled_back BOOLEAN DEFAULT 0
        );
    )";

    if (!pImpl_->execute(sql)) return false;

    // Issues table
    sql = R"(
        CREATE TABLE IF NOT EXISTS issues (
            id TEXT PRIMARY KEY,
            container_id TEXT,
            container_name TEXT,
            category TEXT,
            severity TEXT,
            title TEXT,
            description TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            resolved_at DATETIME
        );
    )";

    if (!pImpl_->execute(sql)) return false;

    // Settings table
    sql = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    if (!pImpl_->execute(sql)) return false;

    return true;
}

bool Database::saveContainer(const docker::Container& container) {
    std::string sql = "INSERT OR REPLACE INTO containers (id, name, image, tag, state, status) VALUES (?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement");
        return false;
    }

    sqlite3_bind_text(stmt, 1, container.getId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, container.getName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, container.getImage().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, container.getImageTag().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, container.getStateString().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, container.getStatus().c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::updateContainer(const docker::Container& container) {
    return saveContainer(container);
}

std::optional<docker::Container> Database::getContainer(const std::string& id) {
    std::string sql = "SELECT id, name, image, tag FROM containers WHERE id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        docker::Container container;
        container.setId(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        container.setName(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        container.setImage(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        container.setImageTag(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));

        sqlite3_finalize(stmt);
        return container;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<docker::Container> Database::getAllContainers() {
    std::vector<docker::Container> containers;
    // TODO: Implement full retrieval
    return containers;
}

bool Database::deleteContainer(const std::string& id) {
    std::string sql = "DELETE FROM containers WHERE id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::recordContainerAction(const ContainerHistory& history) {
    std::string sql = "INSERT INTO container_history (container_id, container_name, action, details, performed_by) VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, history.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, history.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, history.action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, history.details.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, history.performedBy.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<ContainerHistory> Database::getContainerHistory(const std::string& containerId, int limit) {
    return {};
}

std::vector<ContainerHistory> Database::getRecentHistory(int limit) {
    return {};
}

bool Database::recordUpdate(const UpdateHistory& update) {
    std::string sql = "INSERT INTO update_history (container_id, container_name, from_version, to_version, success, error_message) VALUES (?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, update.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, update.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, update.fromVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, update.toVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, update.success ? 1 : 0);
    sqlite3_bind_text(stmt, 6, update.errorMessage.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::vector<UpdateHistory> Database::getUpdateHistory(const std::string& containerId) {
    return {};
}

std::optional<UpdateHistory> Database::getLastUpdate(const std::string& containerId) {
    return std::nullopt;
}

std::vector<UpdateHistory> Database::getFailedUpdates() {
    return {};
}

bool Database::saveIssue(const diagnostics::DiagnosticIssue& issue) {
    std::string sql = "INSERT OR REPLACE INTO issues (id, container_id, container_name, category, severity, title, description) VALUES (?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, issue.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, issue.containerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, issue.containerName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, "network", -1, SQLITE_TRANSIENT); // simplified
    sqlite3_bind_text(stmt, 5, "error", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, issue.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, issue.description.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::updateIssue(const diagnostics::DiagnosticIssue& issue) {
    return saveIssue(issue);
}

std::vector<diagnostics::DiagnosticIssue> Database::getActiveIssues() {
    return {};
}

std::vector<diagnostics::DiagnosticIssue> Database::getResolvedIssues(int limit) {
    return {};
}

bool Database::markIssueResolved(const std::string& issueId) {
    std::string sql = "UPDATE issues SET resolved_at = CURRENT_TIMESTAMP WHERE id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, issueId.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::saveSettings(const AppSettings& settings) {
    // Save each setting individually
    updateSetting("autoUpdate", settings.autoUpdate ? "1" : "0");
    updateSetting("updateSchedule", settings.updateSchedule);
    updateSetting("updateStrategy", settings.updateStrategy);
    return true;
}

std::optional<AppSettings> Database::getSettings() {
    AppSettings settings;
    // TODO: Load settings from database
    settings.autoUpdate = false;
    settings.enableHealthMonitoring = true;
    settings.healthCheckInterval = 60;
    settings.portScanRange = 10000;
    return settings;
}

bool Database::updateSetting(const std::string& key, const std::string& value) {
    std::string sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

int Database::getTotalContainers() {
    std::string sql = "SELECT COUNT(*) FROM containers";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

int Database::getRunningContainers() { return 0; }
int Database::getPendingUpdates() { return 0; }
int Database::getActiveIssuesCount() {
    std::string sql = "SELECT COUNT(*) FROM issues WHERE resolved_at IS NULL";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(pImpl_->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

std::map<std::string, int> Database::getIssuesByCategoryCount() {
    return {};
}

bool Database::cleanupOldHistory(int daysToKeep) {
    std::string sql = "DELETE FROM container_history WHERE timestamp < datetime('now', '-" +
                     std::to_string(daysToKeep) + " days')";
    return pImpl_->execute(sql);
}

bool Database::cleanupResolvedIssues(int daysToKeep) {
    std::string sql = "DELETE FROM issues WHERE resolved_at < datetime('now', '-" +
                     std::to_string(daysToKeep) + " days')";
    return pImpl_->execute(sql);
}

bool Database::vacuum() {
    return pImpl_->execute("VACUUM");
}

bool Database::upgradeSchema(int fromVersion, int toVersion) {
    return true;
}

int Database::getCurrentSchemaVersion() {
    return 1;
}

bool Database::setSchemaVersion(int version) {
    return true;
}

std::string Database::serializeContainer(const docker::Container& container) {
    json j;
    j["id"] = container.getId();
    j["name"] = container.getName();
    j["image"] = container.getImage();
    return j.dump();
}

docker::Container Database::deserializeContainer(const std::string& data) {
    docker::Container container;
    try {
        auto j = json::parse(data);
        container.setId(j.value("id", ""));
        container.setName(j.value("name", ""));
        container.setImage(j.value("image", ""));
    } catch (const json::exception&) {}
    return container;
}

} // namespace storage
