#pragma once
#include <cstddef>
#include <vector>
#include <new>
#include <cstdint>

namespace memory {

class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t capacity = 1024 * 1024); // 1 MB default
    ~ArenaAllocator();

    // Prevent copy and assignment
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // Allocate strictly 64-byte aligned blocks
    void* allocate(std::size_t size);
    void reset(); // O(1) trivial reset, does not deallocate

    std::size_t allocated() const { return m_used; }
    std::size_t capacity() const { return m_capacity; }
    std::size_t allocation_count() const { return m_alloc_count; }

private:
    std::vector<char> m_buffer;
    std::size_t m_capacity = 0;
    std::size_t m_used = 0;
    std::size_t m_alloc_count = 0;
};

} // namespace memory

// Custom placement new and delete using ArenaAllocator
void* operator new(std::size_t size, memory::ArenaAllocator& arena);
void* operator new[](std::size_t size, memory::ArenaAllocator& arena);
void operator delete(void* ptr, memory::ArenaAllocator& arena) noexcept;
void operator delete[](void* ptr, memory::ArenaAllocator& arena) noexcept;
