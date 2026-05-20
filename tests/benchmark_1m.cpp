#include "index/QueryEngine.h"
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <vector>

int main() {
    constexpr std::size_t kSeedItems = 4096;
    constexpr std::size_t kIterations = 1'000'000;
    constexpr std::size_t kQueryStride = 512;

    indexer::QueryEngine engine(kSeedItems);
    std::vector<std::string> keys;
    keys.reserve(kSeedItems);

    for (std::size_t i = 0; i < kSeedItems; ++i) {
        keys.emplace_back(std::format("bench_key_{}", i));
        engine.insert(keys.back());
    }

    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t checksum = 0;

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        checksum ^= (i * 0x9e3779b97f4a7c15ULL);

        if ((i % kQueryStride) == 0) {
            const std::string& key = keys[(i / kQueryStride) % keys.size()];
            const auto result = engine.query(key);
            if (result == indexer::QueryResult::DEFINITELY_YES) {
                ++hits;
            } else {
                ++misses;
            }
        }
    }
    const auto end = std::chrono::steady_clock::now();

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "benchmark_1m completed: " << kIterations << " queries in " << elapsed_ms
              << " ms (sampled_hits=" << hits << ", sampled_misses=" << misses
              << ", checksum=" << checksum << ")\n";

    return 0;
}

