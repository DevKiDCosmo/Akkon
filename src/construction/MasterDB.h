#pragma once
#include "../construction/DatabaseInfo.h"
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <mutex>
#include "sqlite3.h"

namespace construction {

class MasterDB {
public:
    explicit MasterDB(std::filesystem::path dbDir);
    ~MasterDB();

    [[nodiscard]] bool isOpen() const;
    bool ensureOpen();

    bool registerDatabase(const DatabaseInfo& info);
    std::map<int, DatabaseInfo> loadExisting();
    std::vector<DatabaseInfo> getMissing(const std::map<int, DatabaseInfo>& existing) const;
    void recreateMissing(const std::vector<DatabaseInfo>& missing) const;
    // Scan db directory for .db files not present in master and import them as 'unmapped'
    void scanAndImportUnmapped();

    DatabaseInfo createNewDatabase();

    bool insertMetrics(const std::string& timestamp, size_t ram_allocated, size_t ram_runtime, size_t ram_request, size_t ram_capacity, uint64_t disk_space, int reliability, size_t total_queries);
    size_t getLastTotalQueries();

private:
    std::filesystem::path m_dbDir;
    sqlite3* m_db = nullptr;
    mutable std::recursive_mutex m_mutex;
};

} // namespace construction

