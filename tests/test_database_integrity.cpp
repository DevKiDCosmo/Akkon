#include "db/DataStorage.h"
#include "db/Entity.h"
#include "construction/MasterDB.h"
#include "construction/Utils.h"
#include "security/LifecycleManager.h"
#include "monitor/SystemMonitor.h"
#include "sqlite3.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cassert>
#include <filesystem>
#include <random>

struct CsvEntry {
    std::string uuid;
    std::string identifier;
    std::string pwk;
    bool expected_success;
};

std::string generateRandomHex(size_t length) {
    static const char hex_chars[] = "0123456789abcdef";
    static std::mt19937 generator(42); // Fixed seed for reproducibility
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += hex_chars[generator() % 16];
    }
    return result;
}

std::vector<CsvEntry> readCsv(const std::filesystem::path& csvPath) {
    std::vector<CsvEntry> entries;
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open CSV file: " + csvPath.string());
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string uuid, identifier, pwk, expected_str;
        
        // Handle parsing including empty fields
        std::getline(ss, uuid, ',');
        std::getline(ss, identifier, ',');
        std::getline(ss, pwk, ',');
        std::getline(ss, expected_str, ',');
        
        CsvEntry entry;
        entry.uuid = uuid;
        entry.identifier = identifier;
        entry.pwk = pwk;
        entry.expected_success = (expected_str == "1");
        entries.push_back(entry);
    }
    return entries;
}

void writeCsv(const std::filesystem::path& csvPath, const std::vector<CsvEntry>& entries) {
    std::ofstream file(csvPath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not create CSV file: " + csvPath.string());
    }
    for (const auto& entry : entries) {
        file << entry.uuid << ","
             << entry.identifier << ","
             << entry.pwk << ","
             << (entry.expected_success ? "1" : "0") << "\n";
    }
}

class SandboxManager {
public:
    std::filesystem::path path;
    explicit SandboxManager(std::filesystem::path p) : path(std::move(p)) {
        cleanup();
        std::filesystem::create_directories(path);
    }
    ~SandboxManager() {
        cleanup();
    }
    void cleanup() {
        if (std::filesystem::exists(path)) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

bool verifyEntityInShards(const std::vector<std::string>& shard_paths, const CsvEntry& entry) {
    bool found = false;
    for (const auto& path : shard_paths) {
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            continue;
        }
        
        std::string query = "SELECT uuid, identifier, pwk FROM information WHERE identifier = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, entry.identifier.c_str(), -1, SQLITE_TRANSIENT);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* u = sqlite3_column_text(stmt, 0);
                const unsigned char* id = sqlite3_column_text(stmt, 1);
                const unsigned char* p = sqlite3_column_text(stmt, 2);
                
                std::string u_str = u ? reinterpret_cast<const char*>(u) : "";
                std::string id_str = id ? reinterpret_cast<const char*>(id) : "";
                std::string p_str = p ? reinterpret_cast<const char*>(p) : "";
                
                if (u_str == entry.uuid && id_str == entry.identifier && p_str == entry.pwk) {
                    found = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
        if (found) return true;
    }
    return false;
}

int main() {
    std::cout << "=== Akkon 10k Sharding Database Integrity Test ===" << std::endl;

    std::filesystem::path original_cwd = std::filesystem::current_path();
    std::filesystem::path sandbox_path = original_cwd / "test_integrity_sandbox";
    
    // Scoped cleanup
    SandboxManager sandbox(sandbox_path);
    
    // Change directory to sandbox to isolate database files
    std::filesystem::current_path(sandbox_path);

    // 1. Generate 10,000 entries with varying configurations
    std::cout << "Generating 10,000 database entries..." << std::endl;
    std::vector<CsvEntry> generated_entries;
    generated_entries.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        CsvEntry entry;
        if (i < 8000) {
            // 80% have uuid, identifier, and pwk (Full)
            entry.uuid = generateRandomHex(64);
            entry.identifier = generateRandomHex(64);
            entry.pwk = generateRandomHex(64);
            entry.expected_success = true;
        } else if (i < 9000) {
            // 10% have uuid and identifier, empty pwk
            entry.uuid = generateRandomHex(64);
            entry.identifier = generateRandomHex(64);
            entry.pwk = "";
            entry.expected_success = true;
        } else if (i < 9800) {
            // 8% have identifier only (empty uuid, empty pwk)
            entry.uuid = "";
            entry.identifier = generateRandomHex(64);
            entry.pwk = "";
            entry.expected_success = true;
        } else {
            // 2% are invalid: uuid without identifier (empty identifier, empty/non-empty pwk)
            entry.uuid = generateRandomHex(64);
            entry.identifier = "";
            entry.pwk = (i % 2 == 0) ? generateRandomHex(64) : "";
            entry.expected_success = false; // MUST fail constraint validation
        }
        generated_entries.push_back(entry);
    }

    // 2. Write all generated entries to CSV
    std::filesystem::path csv_path = "test_entries.csv";
    std::cout << "Saving entries to CSV: " << csv_path.string() << std::endl;
    writeCsv(csv_path, generated_entries);

    // 3. Read back entries from CSV to ensure exact data match
    std::cout << "Reading entries back from CSV..." << std::endl;
    std::vector<CsvEntry> read_entries = readCsv(csv_path);
    assert(read_entries.size() == 10000 && "CSV entry count mismatch");

    // 4. Set up MasterDB and create 4 database shards
    std::cout << "Initializing MasterDB and creating 4 database shards..." << std::endl;
    std::filesystem::create_directories("db");
    construction::MasterDB master_db("db");
    assert(master_db.isOpen() && "MasterDB failed to open");

    std::vector<std::string> shard_paths;
    for (int i = 0; i < 4; ++i) {
        DatabaseInfo info = master_db.createNewDatabase();
        assert(info.filePos != "ERROR" && "Failed to create new shard database");
        shard_paths.push_back(info.filePos);
        std::cout << "  Created shard " << i << ": " << info.filePos << std::endl;
    }

    // Load registered database shards mapping
    std::map<int, DatabaseInfo> existing_dbs = master_db.loadExisting();
    assert(existing_dbs.size() == 4 && "MasterDB should have registered 4 databases");

    // 5. Initialize DataStorage with the shards mapping
    db::DataStorage data_storage;
    data_storage.initialize(existing_dbs);

    // 6. Insert all 10k entries using DataStorage
    std::cout << "Inserting 10,000 entries into DataStorage..." << std::endl;
    size_t successful_inserts = 0;
    size_t failed_inserts = 0;
    
    for (const auto& entry : read_entries) {
        db::DbEntity entity;
        entity.uuid = entry.uuid;
        entity.identifyer = entry.identifier;
        entity.pwk = entry.pwk;
        
        bool success = data_storage.insertEntity(entity);
        assert(success == entry.expected_success && "Insertion success mismatch against expected constraint check");
        
        if (success) {
            successful_inserts++;
        } else {
            failed_inserts++;
        }
    }

    std::cout << "Insertion complete. Successful: " << successful_inserts 
              << ", Failed/Rejected: " << failed_inserts << std::endl;
              
    assert(successful_inserts == 9800 && "Expected exactly 9,800 successful insertions");
    assert(failed_inserts == 200 && "Expected exactly 200 failed/rejected insertions");

    // 7. Verify entries physically exist in sqlite database files
    std::cout << "Physically verifying every entry from CSV directly in SQLite shards..." << std::endl;
    size_t verified_count = 0;
    for (size_t idx = 0; idx < read_entries.size(); ++idx) {
        const auto& entry = read_entries[idx];
        if (entry.expected_success) {
            bool found = verifyEntityInShards(shard_paths, entry);
            assert(found && "Successfully inserted entry could not be found in any database shard");
            verified_count++;
        } else {
            // Ensure invalid entries are NOT in any shard
            bool found = false;
            // Since identifier is empty, we must search differently (e.g. by uuid)
            for (const auto& path : shard_paths) {
                sqlite3* db = nullptr;
                if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
                    std::string query = "SELECT count(*) FROM information WHERE uuid = ?;";
                    sqlite3_stmt* stmt = nullptr;
                    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(stmt, 1, entry.uuid.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            int count = sqlite3_column_int(stmt, 0);
                            if (count > 0) found = true;
                        }
                        sqlite3_finalize(stmt);
                    }
                    sqlite3_close(db);
                }
            }
            assert(!found && "Rejected entry with empty identifier was incorrectly stored in database");
        }
        
        if (idx > 0 && idx % 2000 == 0) {
            std::cout << "  Verified " << idx << " entries..." << std::endl;
        }
    }

    std::cout << "Verification complete. Verified " << verified_count << " stored entries." << std::endl;

    // Reset current directory back to original CWD before exiting (so RAII SandboxManager cleanup works)
    std::filesystem::current_path(original_cwd);

    std::cout << "=== All database integrity and sharding tests passed! ===" << std::endl;
    return 0;
}
