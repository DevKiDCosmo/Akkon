#pragma once
#include "../construction/DatabaseInfo.h"
#include <string>
#include <map>
#include <vector>
#include <filesystem>
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

private:
    std::filesystem::path m_dbDir;
    sqlite3* m_db = nullptr;
};

} // namespace construction

