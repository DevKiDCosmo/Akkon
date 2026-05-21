#include "DataStorage.h"
#include "../index/QueryEngine.h"
#include "../monitor/SystemMonitor.h"
#include <iostream>
#include <filesystem>

namespace db {

DataStorage::DataStorage() = default;

DataStorage::~DataStorage() {
    for (auto db : m_data_dbs) {
        if (db) sqlite3_close(db);
    }
    m_data_dbs.clear();
}

void DataStorage::initialize(const std::map<int, DatabaseInfo>& mapped_dbs) {
    for (const auto& [idx, info] : mapped_dbs) {
        sqlite3* db = nullptr;
        if (sqlite3_open(info.filePos.c_str(), &db) == SQLITE_OK) {
            sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

            const char* sql = "CREATE TABLE IF NOT EXISTS information (uuid TEXT, identifier TEXT, pwk TEXT);";
            sqlite3_exec(db, sql, nullptr, nullptr, nullptr);

            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_identifier ON information (identifier);", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_uuid ON information (uuid);", nullptr, nullptr, nullptr);

            m_data_dbs.push_back(db);
        } else {
            monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "DataStorage failed to open: " + info.filePos);
        }
    }
}

bool DataStorage::insertEntity(const db::DbEntity& entity, const std::string& tracker_id) {
    if (m_data_dbs.empty()) return false;

    // A identifier can be without a pwk but a uuid never without an identifier. Always.
    if (!entity.uuid.empty() && entity.identifyer.empty()) {
        monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "Write rejected: UUID cannot exist without an identifier.", tracker_id);
        return false;
    }

    // Check disk space before writing (100MB limit)
    try {
        std::filesystem::path dbDir = std::filesystem::current_path() / "db";
        if (std::filesystem::exists(dbDir)) {
            std::filesystem::space_info si = std::filesystem::space(dbDir);
            if (si.available < 100LL * 1024LL * 1024LL) {
                monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "Write rejected: Disk space below 100MB guardrail.", tracker_id);
                return false;
            }
        }
    } catch (const std::exception& e) {
        monitor::SystemMonitor::log(monitor::LogLevel::WARNING, "Failed to check disk space: " + std::string(e.what()), tracker_id);
    }

    sqlite3* target_db = m_data_dbs[m_round_robin_idx];
    if (!target_db) return false;

    const char* filename = sqlite3_db_filename(target_db, "main");
    monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Inserting entity. Target index: " + std::to_string(m_round_robin_idx) + ", File: " + (filename ? filename : "unknown"), tracker_id);

    m_round_robin_idx = (m_round_robin_idx + 1) % m_data_dbs.size();

    const char* sql = "INSERT INTO information (uuid, identifier, pwk) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(target_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "DataStorage prepare statement failed", tracker_id);
        return false;
    }

    sqlite3_bind_text(stmt, 1, entity.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entity.identifyer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, entity.pwk.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "DataStorage insert failed: step rc=" + std::to_string(rc), tracker_id);
        return false;
    }
    return true;
}

void DataStorage::loadAllIdentifiers(indexer::QueryEngine& query_engine) {
    for (auto db : m_data_dbs) {
        if (!db) continue;
        const char* sql = "SELECT identifier FROM information;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* identifier = sqlite3_column_text(stmt, 0);
                if (identifier) {
                    query_engine.insert(reinterpret_cast<const char*>(identifier));
                }
            }
            sqlite3_finalize(stmt);
        }
    }
}

} // namespace db
