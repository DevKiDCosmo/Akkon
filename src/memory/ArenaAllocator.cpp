#include "ArenaAllocator.h"
#include <iostream>

namespace memory {

static constexpr std::size_t ALIGNMENT = 64;

ArenaAllocator::ArenaAllocator(std::size_t capacity)
    : m_capacity(capacity), m_used(0), m_alloc_count(0) {
    // Add extra padding to guarantee we can align within the buffer
    m_buffer.resize(capacity + ALIGNMENT);
}

ArenaAllocator::~ArenaAllocator() = default;

void* ArenaAllocator::allocate(std::size_t size) {
    char* base = m_buffer.data();
    std::uintptr_t curr_addr = reinterpret_cast<std::uintptr_t>(base + m_used);
    
    // Calculate aligned address
    std::uintptr_t aligned_addr = (curr_addr + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    
    // Calculate padding needed to achieve this alignment
    std::size_t padding = aligned_addr - curr_addr;
    
    if (m_used + padding + size > m_capacity) {
        std::cerr << "[ERROR] ArenaAllocator exhausted: " << m_used + padding + size << " > " << m_capacity << std::endl;
        return nullptr;
    }
    
    m_used += padding + size;
    ++m_alloc_count;
    
    return reinterpret_cast<void*>(aligned_addr);
}

void ArenaAllocator::reset() {
    m_used = 0;
    m_alloc_count = 0;
}

} // namespace memory

void* operator new(std::size_t size, memory::ArenaAllocator& arena) {
    return arena.allocate(size);
}

void* operator new[](std::size_t size, memory::ArenaAllocator& arena) {
    return arena.allocate(size);
}

void operator delete(void* ptr, memory::ArenaAllocator& arena) noexcept {
    // ArenaAllocator frees memory en-masse via reset().
    (void)ptr;
    (void)arena;
}

void operator delete[](void* ptr, memory::ArenaAllocator& arena) noexcept {
    (void)ptr;
    (void)arena;
}
