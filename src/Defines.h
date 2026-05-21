#pragma once
#include <cstddef>

namespace akkon {

// Thread Pool Configuration
constexpr size_t THREAD_POOL_SIZE = 8;

// Memory Manager Domain Capacities (in bytes)
constexpr size_t RUNTIME_MEMORY_CAPACITY      = 10 * 1024 * 1024; // 10 MB
constexpr size_t REQUEST_MEMORY_CAPACITY      = 5 * 1024 * 1024;  // 5 MB
constexpr size_t RESERVED_MEMORY_CAPACITY     = 1 * 1024 * 1024;  // 1 MB
constexpr size_t VERIFICATION_MEMORY_CAPACITY = 2 * 1024 * 1024;  // 2 MB
constexpr size_t LIFECYCLE_MEMORY_CAPACITY    = 2 * 1024 * 1024;  // 2 MB

// Global pool defines requested by developer
constexpr size_t RAM_POOL_SIZE  = RUNTIME_MEMORY_CAPACITY;
constexpr size_t MAP_POOL_SIZE  = REQUEST_MEMORY_CAPACITY;
constexpr size_t HEAP_POOL_SIZE = RESERVED_MEMORY_CAPACITY;

} // namespace akkon

