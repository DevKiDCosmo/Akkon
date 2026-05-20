#include "index/QueryEngine.h"
#include <iostream>
#include <cassert>

int main() {
    indexer::QueryEngine engine(100);
    
    engine.insert("user1@example.com");
    engine.insert("user2@example.com");
    
    assert(engine.query("user1@example.com") == indexer::QueryResult::DEFINITELY_YES);
    assert(engine.query("user2@example.com") == indexer::QueryResult::DEFINITELY_YES);
    
    assert(engine.query("user3@example.com") == indexer::QueryResult::DEFINITELY_NO);
    
    std::cout << "BloomFilter / QueryEngine tests passed!\n";
    return 0;
}
