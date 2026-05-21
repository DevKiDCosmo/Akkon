#include "ArenaAllocator.h"
#include "../security/LifecycleManager.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace memory {

static constexpr std::size_t ALIGNMENT = 64;

ArenaAllocator::ArenaAllocator(std::size_t capacity)
    : m_capacity(capacity), m_used(0), m_alloc_count(0) {
    // Add extra padding to guarantee we can align and hold canaries
    // Each allocation consumes 128 bytes for prefix/suffix canaries.
    // Sizing to capacity * 3 + ALIGNMENT ensures the vector never overflows
    // even if many small allocations are performed.
    m_buffer.resize(capacity * 3 + ALIGNMENT);
}

ArenaAllocator::~ArenaAllocator() = default;

void* ArenaAllocator::allocate(std::size_t size) {
    static constexpr std::size_t CANARY_SIZE = 64;
    char* base = m_buffer.data();
    std::uintptr_t curr_addr = reinterpret_cast<std::uintptr_t>(base + m_used);
    
    // Calculate aligned address for the header block
    std::uintptr_t aligned_addr = (curr_addr + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    std::size_t padding = aligned_addr - curr_addr;
    
    // Total physical consumption = header canary + user size + footer canary
    std::size_t consumption = CANARY_SIZE + size + CANARY_SIZE;
    
    if (m_used + padding + consumption > m_capacity) {
        std::cerr << "[ERROR] ArenaAllocator exhausted: " << m_used + padding + consumption << " > " << m_capacity << std::endl;
        return nullptr;
    }
    
    // Write header canary (0xDB)
    char* header_ptr = reinterpret_cast<char*>(aligned_addr);
    std::memset(header_ptr, 0xDB, CANARY_SIZE);
    
    // User block is placed right after the header canary
    char* user_ptr = header_ptr + CANARY_SIZE;
    
    // Write footer canary (0xDB)
    char* footer_ptr = user_ptr + size;
    std::memset(footer_ptr, 0xDB, CANARY_SIZE);
    
    m_used += padding + consumption;
    ++m_alloc_count;
    
    // Track allocation metadata
    m_allocs.push_back({ static_cast<std::size_t>(user_ptr - base), size });
    
    return reinterpret_cast<void*>(user_ptr);
}

void ArenaAllocator::reset() {
    m_used = 0;
    m_alloc_count = 0;
    m_allocs.clear();
}

void ArenaAllocator::verify() const {
    static constexpr std::size_t CANARY_SIZE = 64;
    const char* base = m_buffer.data();
    for (const auto& alloc : m_allocs) {
        const char* user_ptr = base + alloc.offset;
        const char* header_ptr = user_ptr - CANARY_SIZE;
        const char* footer_ptr = user_ptr + alloc.size;
        
        // Verify header canary
        for (std::size_t i = 0; i < CANARY_SIZE; ++i) {
            if (static_cast<unsigned char>(header_ptr[i]) != 0xDB) {
                std::cerr << "[ALARM] Memory corruption detected! Canary corrupted at header prefix." << std::endl;
                security::LifecycleManager::triggerLockdown(security::LockdownState::IMMEDIATE);
                throw std::runtime_error("Memory corruption: Canary check failed!");
            }
        }
        
        // Verify footer canary
        for (std::size_t i = 0; i < CANARY_SIZE; ++i) {
            if (static_cast<unsigned char>(footer_ptr[i]) != 0xDB) {
                std::cerr << "[ALARM] Memory corruption detected! Canary corrupted at footer suffix." << std::endl;
                security::LifecycleManager::triggerLockdown(security::LockdownState::IMMEDIATE);
                throw std::runtime_error("Memory corruption: Canary check failed!");
            }
        }
    }
}

void ArenaAllocator::check_access(const void* ptr, std::size_t size) const {
    if (!ptr) return;
    const char* p = reinterpret_cast<const char*>(ptr);
    const char* start = m_buffer.data();
    const char* end = start + m_capacity;
    if (p < start || p + size > end) {
        std::cerr << "[ALARM] Memory access violation: Address out of designated space!" << std::endl;
        security::LifecycleManager::triggerLockdown(security::LockdownState::IMMEDIATE);
        throw std::runtime_error("Memory access violation: Address out of designated space!");
    }
}

} // namespace memory

void* operator new(std::size_t size, memory::ArenaAllocator& arena) {
    return arena.allocate(size);
}

void* operator new[](std::size_t size, memory::ArenaAllocator& arena) {
    return arena.allocate(size);
}

void operator delete(void* ptr, memory::ArenaAllocator& arena) noexcept {
    (void)ptr;
    (void)arena;
}

void operator delete[](void* ptr, memory::ArenaAllocator& arena) noexcept {
    (void)ptr;
    (void)arena;
}

