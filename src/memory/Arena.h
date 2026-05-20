#pragma once
#include <cstddef>
#include <vector>

namespace memory {

class Arena {
public:
    explicit Arena(std::size_t capacity = 1024 * 1024); // 1 MB default
    ~Arena();

    void* allocate(std::size_t size, std::size_t align = alignof(std::max_align_t));
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

