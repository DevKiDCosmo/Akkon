#include "DataStorage.h"
#include "../index/QueryEngine.h"
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
            const char* sql = "CREATE TABLE IF NOT EXISTS information (uuid TEXT, identifier TEXT, pwk TEXT);";
            sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
            m_data_dbs.push_back(db);
        } else {
            std::cerr << "[ERROR] DataStorage failed to open: " << info.filePos << std::endl;
        }
    }
}

bool DataStorage::insertEntity(const db::DbEntity& entity) {
    if (m_data_dbs.empty()) return false;

    // Check disk space before writing (100MB limit)
    try {
        std::filesystem::path dbDir = std::filesystem::current_path() / "db";
        if (std::filesystem::exists(dbDir)) {
            std::filesystem::space_info si = std::filesystem::space(dbDir);
            if (si.available < 100LL * 1024LL * 1024LL) {
                std::cerr << "[ERROR] Write rejected: Disk space below 100MB guardrail." << std::endl;
                return false;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Failed to check disk space: " << e.what() << std::endl;
    }

    sqlite3* target_db = m_data_dbs[m_round_robin_idx];
    if (!target_db) return false;

    const char* filename = sqlite3_db_filename(target_db, "main");
    std::cout << "[DEBUG] Inserting entity. Target index: " << m_round_robin_idx 
              << ", File: " << (filename ? filename : "unknown") << std::endl;

    m_round_robin_idx = (m_round_robin_idx + 1) % m_data_dbs.size();

    std::string sql = "INSERT INTO information (uuid, identifier, pwk) VALUES ('" + 
                      entity.uuid + "', '" + entity.identifyer + "', '" + entity.pwk + "');";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(target_db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            std::cerr << "[ERROR] DataStorage insert failed: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
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
