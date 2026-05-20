#include "memory/MemoryManager.h"
#include "db/Entity.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <cmath>

void test_isolation(memory::MemoryManager& mem) {
    auto runtime = mem.getDomain(memory::RuntimeDomain::RUNTIME);
    auto request = mem.getDomain(memory::RuntimeDomain::REQUEST);
    
    assert(runtime != nullptr);
    assert(request != nullptr);
    
    void* ptr1 = runtime->allocate(sizeof(db::DbEntity));
    assert(ptr1 != nullptr);
    
    void* ptr2 = request->allocate(sizeof(db::DbEntity));
    assert(ptr2 != nullptr);
    
    std::ptrdiff_t diff = static_cast<char*>(ptr1) - static_cast<char*>(ptr2);
    // Because they are in entirely separate pre-allocated arenas (10MB vs 5MB buffers),
    // they must not be contiguous or overlapping.
    assert(std::abs(diff) >= 5 * 1024 * 1024);
}

int main() {
    memory::MemoryManager mem;
    
    std::thread t1(test_isolation, std::ref(mem));
    t1.join();
    
    std::cout << "Memory isolation tests passed!\n";
    return 0;
}
