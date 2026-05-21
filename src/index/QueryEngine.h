#pragma once
#include "BloomFilter.h"
#include <string>
#include <unordered_set>
#include "../memory/ArenaAllocator.h"

namespace indexer {

enum class QueryResult {
    DEFINITELY_NO,
    PROBABLY_YES,
    DEFINITELY_YES
};

class QueryEngine {
public:
    QueryEngine(std::size_t expected_items, memory::ArenaAllocator* arena = nullptr);

    void insert(const std::string& item);
    QueryResult query(const std::string& item) const;

private:
    BloomFilter m_filter;
    std::unordered_set<
        std::string,
        std::hash<std::string>,
        std::equal_to<std::string>,
        memory::ArenaAllocatorSTL<std::string>
    > m_exact_store;
    memory::ArenaAllocator* m_arena;
};

} // namespace indexer
