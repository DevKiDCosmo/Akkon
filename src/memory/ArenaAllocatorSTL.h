#pragma once
#include "ArenaAllocator.h"
#include <new>
#include <cstdlib>

namespace memory {

template <typename T>
class ArenaAllocatorSTL {
public:
    using value_type = T;

    ArenaAllocatorSTL() noexcept : m_arena(nullptr) {}
    ArenaAllocatorSTL(memory::ArenaAllocator* arena) noexcept : m_arena(arena) {}

    template <typename U>
    ArenaAllocatorSTL(const ArenaAllocatorSTL<U>& other) noexcept : m_arena(other.m_arena) {}

    T* allocate(std::size_t n) {
        if (!m_arena) {
            void* ptr = std::malloc(n * sizeof(T));
            if (!ptr) throw std::bad_alloc();
            return static_cast<T*>(ptr);
        }
        void* ptr = m_arena->allocate(n * sizeof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (!m_arena) {
            std::free(p);
        }
        // If m_arena is set, memory is reclaimed en-masse upon reset()
        (void)p;
        (void)n;
    }

    template <typename U>
    bool operator==(const ArenaAllocatorSTL<U>& other) const noexcept {
        return m_arena == other.m_arena;
    }

    template <typename U>
    bool operator!=(const ArenaAllocatorSTL<U>& other) const noexcept {
        return m_arena != other.m_arena;
    }

    memory::ArenaAllocator* m_arena;
};

} // namespace memory
