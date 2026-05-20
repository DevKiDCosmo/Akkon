#pragma once
#include "BloomFilter.h"
#include <string>
#include <unordered_set>

namespace indexer {

enum class QueryResult {
    DEFINITELY_NO,
    PROBABLY_YES,
    DEFINITELY_YES
};

class QueryEngine {
public:
    QueryEngine(std::size_t expected_items);

    void insert(const std::string& item);
    QueryResult query(const std::string& item) const;

private:
    BloomFilter m_filter;
    std::unordered_set<std::string> m_exact_store;
};

} // namespace indexer
