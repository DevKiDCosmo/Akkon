#pragma once

namespace memory {

enum class RuntimeDomain {
    RUNTIME,      // Static resources necessary to run
    REQUEST,      // Dynamic allocation for incoming queries
    RESERVED,     // Reserved for future extensions
    VERIFICATION, // Special domain for online vulnerability data
    LIFECYCLE     // Special domain for security state and lockdown handling
};

} // namespace memory
