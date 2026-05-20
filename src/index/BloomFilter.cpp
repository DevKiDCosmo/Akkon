#include "BloomFilter.h"
#include "../crypto/Hash.h"

namespace indexer {

BloomFilter::BloomFilter(std::size_t size_in_bits, uint8_t num_hash_functions)
    : m_bits(size_in_bits, false), m_num_hashes(num_hash_functions) {}

std::vector<std::size_t> BloomFilter::hash(const std::string& item) const {
    std::vector<std::size_t> hashes;
    hashes.reserve(m_num_hashes);
    
    std::string sha256_hex = crypto::Hash::sha256(item);
    
    // Extract 32-bit chunks from the 256-bit hash (up to 8 hashes)
    for (uint8_t i = 0; i < m_num_hashes && i < 8; ++i) {
        std::string chunk = sha256_hex.substr(i * 8, 8);
        std::size_t h = std::stoull(chunk, nullptr, 16);
        hashes.push_back(h % m_bits.size());
    }
    
    return hashes;
}

void BloomFilter::add(const std::string& item) {
    auto hashes = hash(item);
    for (auto h : hashes) {
        m_bits[h] = true;
    }
}

bool BloomFilter::possiblyContains(const std::string& item) const {
    auto hashes = hash(item);
    for (auto h : hashes) {
        if (!m_bits[h]) {
            return false;
        }
    }
    return true;
}

} // namespace indexer
