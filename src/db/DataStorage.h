#pragma once
#include "Entity.h"
#include "../construction/DatabaseInfo.h"
#include <vector>
#include <map>
#include <string>
#include "sqlite3.h"

namespace indexer {
    class QueryEngine;
}

namespace db {

class DataStorage {
public:
    DataStorage();
    ~DataStorage();

    void initialize(const std::map<int, DatabaseInfo>& mapped_dbs);
    bool insertEntity(const db::DbEntity& entity, const std::string& tracker_id = "SYSTEM");
    void loadAllIdentifiers(indexer::QueryEngine& query_engine);

private:
    std::vector<sqlite3*> m_data_dbs;
    size_t m_round_robin_idx = 0;
};

} // namespace db
