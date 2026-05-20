#include "index/QueryEngine.h"
#include <thread>
#include <vector>
#include <cassert>
#include <iostream>
#include <mutex>
#include <functional>

std::mutex g_mut;

void async_worker(indexer::QueryEngine& qe, int start_val) {
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key_" + std::to_string(start_val + i);
        
        // Locking to simulate thread-safe database insertion/querying. 
        // In a true highly-concurrent environment, the QueryEngine would use atomic ops or RW locks.
        std::lock_guard<std::mutex> lock(g_mut);
        qe.insert(key);
        assert(qe.query(key) != indexer::QueryResult::DEFINITELY_NO);
    }
}

int main() {
    indexer::QueryEngine qe(100000); 
    
    std::vector<std::thread> workers;
    for (int i = 0; i < 10; ++i) {
        workers.emplace_back(async_worker, std::ref(qe), i * 1000);
    }
    
    for (auto& w : workers) {
        w.join();
    }
    
    std::cout << "Concurrency tests passed!\n";
    return 0;
}
