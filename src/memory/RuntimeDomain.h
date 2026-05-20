#pragma once

namespace memory {

enum class RuntimeDomain {
    RUNTIME_META,     // Program state: DB registry, config, metadata
    RUNTIME_REQUEST,  // Network request payloads, temporary data
};

} // namespace memory

