#include "Arena.h"
#include <cstring>
#include <iostream>

namespace memory {

Arena::Arena(std::size_t capacity)
    : m_capacity(capacity), m_used(0), m_alloc_count(0) {
    m_buffer.resize(capacity);
}

Arena::~Arena() = default;

void* Arena::allocate(std::size_t size, std::size_t align) {
    // Align the current position
    std::size_t aligned_used = (m_used + align - 1) / align * align;

    if (aligned_used + size > m_capacity) {
        std::cerr << "[ERROR] Arena exhausted: " << aligned_used + size << " > " << m_capacity << std::endl;
        return nullptr;
    }

    void* ptr = m_buffer.data() + aligned_used;
    m_used = aligned_used + size;
    ++m_alloc_count;
    return ptr;
}

void Arena::reset() {
    m_used = 0;
    m_alloc_count = 0;
}

} // namespace memory

