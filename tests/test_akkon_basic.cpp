#include "testsuite/TestFramework.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace testsuite;

int main() {
    std::cout << "=== Akkon TestFramework Basic Test ===" << std::endl;

    TestFramework framework;

    // Test 1: Create instance
    std::cout << "\n[TEST 1] Creating Akkon instance with -c 1..." << std::endl;
    std::vector<std::string> args = {"-c", "1"};
    std::string tracker_id = framework.createInstance(args);
    assert(!tracker_id.empty() && "Failed to create instance");
    std::cout << "  [PASS] Instance created with tracker_id: " << tracker_id << std::endl;

    // Test 2: Get tracking info
    std::cout << "\n[TEST 2] Getting tracking info..." << std::endl;
    auto info = framework.getTracking(tracker_id);
    assert(info.pid >= 0 && "Invalid PID");
    assert(info.tracker_id == tracker_id && "Tracker ID mismatch");
    std::cout << "  [PASS] Tracking info retrieved" << std::endl;
    std::cout << "    PID: " << info.pid << std::endl;
    std::cout << "    Log: " << info.log_file << std::endl;

    // Test 3: Get status
    std::cout << "\n[TEST 3] Getting instance status..." << std::endl;
    auto status = framework.getStatus(tracker_id);
    assert(status.pid == info.pid && "PID status mismatch");
    std::cout << "  [PASS] Status retrieved" << std::endl;
    std::cout << "    Uptime: " << status.uptime_ms << " ms" << std::endl;
    std::cout << "    Status: " << status.status << std::endl;

    // Test 4: Wait for instance
    std::cout << "\n[TEST 4] Waiting for instance to complete..." << std::endl;
    bool stopped = framework.stopInstance(tracker_id);
    assert(stopped && "Failed to stop instance");
    bool waited = framework.waitForInstance(tracker_id, 10000);
    assert(waited && "Instance did not complete in time");
    std::cout << "  [PASS] Instance completed" << std::endl;

    // Test 5: Get exit code
    std::cout << "\n[TEST 5] Getting exit code..." << std::endl;
    auto exit_code = framework.getExitCode(tracker_id);
    std::cout << "  [PASS] Exit code: " << exit_code << std::endl;

    // Test 6: Get logs
    std::cout << "\n[TEST 6] Reading logs..." << std::endl;
    auto logs = framework.getLogs(tracker_id, 10);
    std::cout << "  [PASS] Retrieved " << logs.size() << " log lines" << std::endl;
    if (!logs.empty()) {
        std::cout << "    Last log line: " << logs.back() << std::endl;
    }

    // Test 7: List instances
    std::cout << "\n[TEST 7] Listing instances..." << std::endl;
    auto instances = framework.listInstances();
    assert(instances.size() >= 1 && "Instance not in list");
    std::cout << "  [PASS] Found " << instances.size() << " instance(s)" << std::endl;

    std::cout << "\n=== All TestFramework tests passed! ===" << std::endl;

    return 0;
}

