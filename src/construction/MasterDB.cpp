#include "MasterDB.h"
#include "Utils.h"
#include <iostream>
#include <set>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <utility>

namespace construction {

MasterDB::MasterDB(std::filesystem::path dbDir)
    : m_dbDir(std::move(dbDir)) {
    ensureOpen();
}

MasterDB::~MasterDB() {
    if (m_db) sqlite3_close(m_db);
}

bool MasterDB::isOpen() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_db != nullptr;
}

bool MasterDB::ensureOpen() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_db) return true;
    std::filesystem::path path = m_dbDir / "master.db";
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc) {
        std::cerr << "[ERROR] Can't open master database: " << sqlite3_errmsg(m_db) << std::endl;
        if (m_db) sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    char* errMsg = nullptr;
    const char* sql = "CREATE TABLE IF NOT EXISTS databases ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "uuid TEXT UNIQUE NOT NULL,"
                      "creation_date TEXT NOT NULL,"
                      "file_path TEXT NOT NULL,"
                      "status TEXT DEFAULT 'active');";
    rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    const char* sqlMetrics = "CREATE TABLE IF NOT EXISTS metrics ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                             "timestamp TEXT NOT NULL,"
                             "ram_allocated INTEGER NOT NULL,"
                             "ram_runtime INTEGER NOT NULL,"
                             "ram_request INTEGER NOT NULL,"
                             "ram_capacity INTEGER NOT NULL,"
                             "disk_space INTEGER NOT NULL,"
                             "reliability INTEGER NOT NULL,"
                             "total_queries INTEGER NOT NULL"
                             ");";
    rc = sqlite3_exec(m_db, sqlMetrics, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error creating metrics table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Backward compatibility: normalize old status labels
    rc = sqlite3_exec(m_db, "UPDATE databases SET status='unmapped' WHERE status='external';", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL migration error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    return true;
}

bool MasterDB::registerDatabase(const DatabaseInfo& info) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!ensureOpen()) return false;
    char* errMsg = nullptr;
    std::string escapedPath = escapeSqlString(info.filePos);
    std::string insertSql = "INSERT OR IGNORE INTO databases (uuid, creation_date, file_path, status) VALUES ('" +
                            info.uuid + "', '" + info.creationDate + "', '" + escapedPath + "', '" +
                            (info.status.empty() ? std::string("active") : info.status) + "');";
    int rc = sqlite3_exec(m_db, insertSql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error registering database: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::map<int, DatabaseInfo> MasterDB::loadExisting() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::map<int, DatabaseInfo> databaseMap;
    if (!ensureOpen()) return databaseMap;
    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT id, uuid, creation_date, file_path, status FROM databases ORDER BY id;";
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        int index = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DatabaseInfo info;
            info.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            info.creationDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            info.filePos = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            info.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            index++;
            databaseMap[index] = info;
        }
    }
    sqlite3_finalize(stmt);
    return databaseMap;
}

std::vector<DatabaseInfo> MasterDB::getMissing(const std::map<int, DatabaseInfo>& existing) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<DatabaseInfo> missing;
    for (const auto& [idx, info] : existing) {
        if (!std::filesystem::exists(info.filePos)) {
            std::cout << "[WARN] Database not found: " << info.uuid << " at " << info.filePos << std::endl;
            missing.push_back(info);
        }
    }
    return missing;
}

void MasterDB::recreateMissing(const std::vector<DatabaseInfo>& missing) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& info : missing) {
        std::cout << "[INFO] Recreating: " << info.uuid << std::endl;
        std::filesystem::path dbPath = m_dbDir / (info.uuid + ".db");
        sqlite3* db = nullptr;
        char* errMsg = nullptr;
        int rc = sqlite3_open(dbPath.c_str(), &db);
        if (rc) {
            std::cerr << "[ERROR] Can't open database: " << sqlite3_errmsg(db) << std::endl;
            if (db) sqlite3_close(db);
            continue;
        }
        const char* sql = "CREATE TABLE IF NOT EXISTS metadata (uuid TEXT PRIMARY KEY, creation_date TEXT, file_path TEXT);";
        rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "[ERROR] SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        
        // Ensure information table exists
        const char* sqlData = "CREATE TABLE IF NOT EXISTS information (uuid TEXT, identifier TEXT, pwk TEXT);";
        sqlite3_exec(db, sqlData, nullptr, nullptr, nullptr);

        std::string insertSql = "INSERT OR REPLACE INTO metadata VALUES ('" + info.uuid + "', '" + info.creationDate + "', '" + escapeSqlString(dbPath.string()) + "');";
        rc = sqlite3_exec(db, insertSql.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "[ERROR] SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
        sqlite3_close(db);
    }
}

void MasterDB::scanAndImportUnmapped() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!ensureOpen()) return;
    // Collect existing file paths from master DB
    std::set<std::string, std::less<>> mappedPaths;
    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT file_path FROM databases;";
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(stmt, 0);
            if (txt) mappedPaths.emplace(reinterpret_cast<const char*>(txt));
        }
    }
    sqlite3_finalize(stmt);

    for (auto& entry : std::filesystem::directory_iterator(m_dbDir)) {
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        if (p.filename() == "master.db") continue;
        if (p.extension() != ".db") continue;
        std::string full = p.string();
        if (mappedPaths.contains(full)) continue; // already mapped

        // Try to read metadata from the DB
        DatabaseInfo info;
        info.filePos = full;
        sqlite3* db = nullptr;
        if (sqlite3_open(full.c_str(), &db) == SQLITE_OK) {
            sqlite3_stmt* s = nullptr;
            const char* q = "SELECT uuid, creation_date FROM metadata LIMIT 1;";
            if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                if (sqlite3_step(s) == SQLITE_ROW) {
                    const unsigned char* u = sqlite3_column_text(s, 0);
                    const unsigned char* c = sqlite3_column_text(s, 1);
                    if (u) info.uuid = reinterpret_cast<const char*>(u);
                    if (c) info.creationDate = reinterpret_cast<const char*>(c);
                }
            }
            sqlite3_finalize(s);
            sqlite3_close(db);
        }

        // If metadata not present, derive uuid from filename (stem) and timestamp from file mtime
        if (info.uuid.empty()) info.uuid = p.stem().string();
        if (info.creationDate.empty()) {
            auto ftime = std::filesystem::last_write_time(p);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
            std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm{};
            localtime_r(&cftime, &tm);
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            info.creationDate = ss.str();
        }

        // Insert into master DB with status 'external'
        char* errMsg = nullptr;
        std::string insert = "INSERT INTO databases (uuid, creation_date, file_path, status) VALUES ('" +
                             escapeSqlString(info.uuid) + "', '" + escapeSqlString(info.creationDate) + "', '" + escapeSqlString(full) + "', 'unmapped');";
        int rc = sqlite3_exec(m_db, insert.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "[ERROR] Failed to import external DB " << full << ": " << errMsg << std::endl;
            sqlite3_free(errMsg);
        } else {
            std::cout << "[INFO] Imported unmapped DB: " << full << " as uuid=" << info.uuid << std::endl;
        }
    }
}

DatabaseInfo MasterDB::createNewDatabase() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    DatabaseInfo info;
    info.uuid = generateUUID();
    info.creationDate = getCurrentTimestamp();
    std::filesystem::path dbPath = m_dbDir / (info.uuid + ".db");
    sqlite3* db = nullptr;
    char* errMsg = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc) {
        std::cerr << "[ERROR] Can't open database: " << sqlite3_errmsg(db) << std::endl;
        if (db) sqlite3_close(db);
        info.filePos = "ERROR";
        return info;
    }
    const char* sql = "CREATE TABLE IF NOT EXISTS metadata (uuid TEXT PRIMARY KEY, creation_date TEXT, file_path TEXT);";
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    
    // Ensure information table exists
    const char* sqlData = "CREATE TABLE IF NOT EXISTS information (uuid TEXT, identifier TEXT, pwk TEXT);";
    sqlite3_exec(db, sqlData, nullptr, nullptr, nullptr);

    std::string insertSql = "INSERT INTO metadata VALUES ('" + info.uuid + "', '" + info.creationDate + "', '" + escapeSqlString(dbPath.string()) + "');";
    rc = sqlite3_exec(db, insertSql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    
    sqlite3_close(db);
    info.filePos = dbPath.string();
    info.status = "active";
    registerDatabase(info);
    return info;
}



bool MasterDB::insertMetrics(const std::string& timestamp, size_t ram_allocated, size_t ram_runtime, size_t ram_request, size_t ram_capacity, uint64_t disk_space, int reliability, size_t total_queries) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!ensureOpen()) return false;
    
    char* errMsg = nullptr;
    std::string sql = "INSERT INTO metrics (timestamp, ram_allocated, ram_runtime, ram_request, ram_capacity, disk_space, reliability, total_queries) VALUES ('"
        + escapeSqlString(timestamp) + "', "
        + std::to_string(ram_allocated) + ", "
        + std::to_string(ram_runtime) + ", "
        + std::to_string(ram_request) + ", "
        + std::to_string(ram_capacity) + ", "
        + std::to_string(disk_space) + ", "
        + std::to_string(reliability) + ", "
        + std::to_string(total_queries) + ");";
        
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQL error inserting metrics: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

size_t MasterDB::getLastTotalQueries() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!ensureOpen()) return 0;
    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT total_queries FROM metrics ORDER BY id DESC LIMIT 1;";
    size_t last_total = 0;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            last_total = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    return last_total;
}

} // namespace construction

