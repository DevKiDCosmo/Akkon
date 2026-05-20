#include "QueryEngine.h"

namespace indexer {

QueryEngine::QueryEngine(std::size_t expected_items)
    : m_filter(expected_items * 10, 7) {}

void QueryEngine::insert(const std::string& item) {
    m_filter.add(item);
    m_exact_store.insert(item);
}

QueryResult QueryEngine::query(const std::string& item) const {
    if (!m_filter.possiblyContains(item)) {
        return QueryResult::DEFINITELY_NO;
    }
    
    if (m_exact_store.find(item) != m_exact_store.end()) {
        return QueryResult::DEFINITELY_YES;
    }
    
    return QueryResult::PROBABLY_YES; // false positive
}

} // namespace indexer
