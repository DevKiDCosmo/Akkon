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
    explicit MasterDB(const std::filesystem::path& dbDir);
    ~MasterDB();

    bool isOpen() const;
    bool ensureOpen();

    bool registerDatabase(const DatabaseInfo& info);
    std::map<int, DatabaseInfo> loadExisting();
    std::vector<DatabaseInfo> getMissing(const std::map<int, DatabaseInfo>& existing);
    void recreateMissing(const std::vector<DatabaseInfo>& missing);
    // Scan db directory for .db files not present in master and import them as 'external'
    void scanAndImportExternal();

    DatabaseInfo createNewDatabase();

private:
    std::filesystem::path m_dbDir;
    sqlite3* m_db = nullptr;
};

} // namespace construction

