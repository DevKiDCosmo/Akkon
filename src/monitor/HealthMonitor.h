#pragma once
#include <cstdint>
#include <cstddef>

namespace monitor {

struct HealthStats {
    uint64_t available_disk_bytes;
    int reliability_score; // 0-100
};

class HealthMonitor {
public:
    static HealthStats checkHealth(size_t ram_allocated = 0, size_t ram_capacity = 0);
};

} // namespace monitor
