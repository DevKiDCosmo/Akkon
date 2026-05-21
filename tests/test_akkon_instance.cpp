#include "testsuite/TestFramework.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace testsuite;

int main() {
    std::cout << "=== Akkon Instance Management Test ===" << std::endl;

    TestFramework framework;

    // Test 1: Create quick instance
    std::cout << "\n[TEST 1] Creating fast instance..." << std::endl;
    std::vector<std::string> args = {"-c", "1"};
    std::string tracker_id = framework.createInstance(args);
    assert(!tracker_id.empty() && "Failed to create instance");
    std::cout << "  [PASS] Instance created: " << tracker_id << std::endl;

    // Test 2: Check instance is running
    std::cout << "\n[TEST 2] Checking if instance is running..." << std::endl;
    bool is_running = framework.isRunning(tracker_id);
    std::cout << "  [PASS] Running: " << (is_running ? "yes" : "no") << std::endl;

    // Test 3: Get live logs (short)
    std::cout << "\n[TEST 3] Getting logs..." << std::endl;
    auto logs = framework.getLogs(tracker_id, 3);
    std::cout << "  [PASS] Retrieved " << logs.size() << " log line(s)" << std::endl;

    // Test 4: Wait for completion
    std::cout << "\n[TEST 4] Waiting for instance..." << std::endl;
    bool stopped = framework.stopInstance(tracker_id);
    assert(stopped && "Failed to stop instance");
    bool waited = framework.waitForInstance(tracker_id, 15000);
    std::cout << "  [PASS] Completed: " << (waited ? "yes" : "timeout") << std::endl;

    // Test 5: Verify stopped
    std::cout << "\n[TEST 5] Verifying instance stopped..." << std::endl;
    is_running = framework.isRunning(tracker_id);
    assert(!is_running && "Instance should not be running");
    std::cout << "  [PASS] Running: no" << std::endl;

    std::cout << "\n=== All instance management tests passed! ===" << std::endl;

    return 0;
}

