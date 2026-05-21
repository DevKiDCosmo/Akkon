# Akkon Tests

The test suite includes unit tests, integration tests, and benchmarks.

## Quick Start

```bash
# Run all tests
./run_all_tests.sh

# Run specific test category
./run_integration_tests.sh
./run_unit_tests.sh
./run_benchmarks.sh

# Run a single test
./run_test.sh test_akkon_basic
```

## Test Helper Scripts

- `run_all_tests.sh` — Run all tests
- `run_unit_tests.sh` — Run only unit tests
- `run_integration_tests.sh` — Run only integration tests
- `run_benchmarks.sh` — Run only benchmark tests
- `run_test.sh <name>` — Run a specific test by name

## Test Categories

### Unit Tests
- `test_arena_allocator` — Memory arena allocation
- `test_bloom_filter` — Bloom filter and query engine
- `test_network` — Network server communication
- `test_availability` — Server availability checks
- `test_concurrency` — Concurrent query operations
- `test_memory_isolation` — Memory domain isolation
- `test_hashing` — SHA256 hashing

### Integration Tests (using TestFramework)
- `test_akkon_basic` — Basic instance creation and lifecycle
- `test_akkon_instance` — Instance management and process tracking

### Benchmarks
- `benchmark_1m` — 1 million query performance test

## TestFramework

The `testsuite::TestFramework` class provides:
- **createInstance(args)** — Start a new Akkon process
- **getTracking(id)** — Get instance metadata
- **getStatus(id)** — Get current process status
- **getLogs(id)** — Read process logs
- **stopInstance(id)** — Stop a process gracefully
- **killInstance(id)** — Forcefully kill a process
- **waitForInstance(id)** — Wait for process completion
- **isRunning(id)** — Check if instance is running
- **getExitCode(id)** — Get process exit code

## Example Usage

```cpp
#include "testsuite/TestFramework.h"

using namespace testsuite;

int main() {
    TestFramework framework;
    
    // Create instance
    auto tracker_id = framework.createInstance({"-c", "2"});
    
    // Get status
    auto status = framework.getStatus(tracker_id);
    std::cout << "PID: " << status.pid << " Uptime: " << status.uptime_ms << "ms" << std::endl;
    
    // Wait for completion
    framework.waitForInstance(tracker_id, 10000);
    
    // Get logs
    auto logs = framework.getLogs(tracker_id, 5);
    
    return 0;
}
```

## Logs

Test logs are stored in `cmake-build-debug/test_logs/` with tracker IDs as filenames.

Example: `cmake-build-debug/test_logs/f35b8167.log`

## Deprecated Tests

Old Python WebSocket tests are in `deprecated/` and are no longer maintained.

