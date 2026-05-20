#include "memory/ArenaAllocator.h"
#include <iostream>
#include <cassert>

struct TestObject {
    int x;
    double y;
};

int main() {
    memory::ArenaAllocator arena(1024);
    
    void* ptr1 = arena.allocate(10);
    assert(reinterpret_cast<std::uintptr_t>(ptr1) % 64 == 0);
    
    void* ptr2 = arena.allocate(20);
    assert(reinterpret_cast<std::uintptr_t>(ptr2) % 64 == 0);
    
    TestObject* obj = new(arena) TestObject{42, 3.14};
    assert(reinterpret_cast<std::uintptr_t>(obj) % 64 == 0);
    assert(obj->x == 42);
    
    arena.reset();
    assert(arena.allocated() == 0);
    assert(arena.allocation_count() == 0);
    
    std::cout << "ArenaAllocator tests passed!\n";
    return 0;
}
