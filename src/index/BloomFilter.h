#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "../memory/ArenaAllocator.h"

namespace indexer {

class BloomFilter {
public:
    BloomFilter(std::size_t size_in_bits, uint8_t num_hash_functions, memory::ArenaAllocator* arena = nullptr);

    void add(const std::string& item);
    bool possiblyContains(const std::string& item) const;

private:
    std::vector<bool, memory::ArenaAllocatorSTL<bool>> m_bits;
    uint8_t m_num_hashes;
    memory::ArenaAllocator* m_arena;

    std::vector<std::size_t> hash(const std::string& item) const;
};

} // namespace indexer
