///
/// Example: Simplified Akkon testing using runtime()
///
/// The new runtime() method consolidates all instance management into a single call:
/// - Detects containerization (Docker, Podman, LXC, etc.)
/// - Sets up environment variables automatically
/// - Creates, runs, and cleans up the instance
/// - Returns exit code directly
///
/// This significantly reduces boilerplate code in test files.
///

#include "testsuite/TestFramework.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace testsuite;

int main() {
    std::cout << "=== Example: Using runtime() for simplified testing ===" << std::endl;

    TestFramework framework;

    // ===== OLD WAY (Before runtime()) =====
    // std::vector<std::string> args = {"-c", "1"};
    // std::string tracker_id = framework.createInstance(args);
    // assert(!tracker_id.empty() && "Failed to create instance");
    //
    // bool completed = framework.waitForInstance(tracker_id, 10000);
    // assert(completed && "Instance did not complete");
    //
    // std::int32_t exit_code = framework.getExitCode(tracker_id);
    // auto logs = framework.getLogs(tracker_id, 10);
    //
    // framework.stopInstance(tracker_id);

    // ===== NEW WAY (Using runtime()) =====
    std::vector<std::string> args = {"-c", "1"};

    // Optional: Set custom environment variables
    std::map<std::string, std::string> env = {
        {"AKKON_DEBUG", "1"},
        {"AKKON_LOG_LEVEL", "INFO"}
    };

    // Run everything in one call!
    std::int32_t exit_code = framework.runtime(args, env);

    // Check result
    if (exit_code == 0) {
        std::cout << "\n[SUCCESS] Instance completed successfully!" << std::endl;
    } else {
        std::cout << "\n[INFO] Instance completed with exit code: " << exit_code << std::endl;
    }

    std::cout << "\n=== Benefits of using runtime() ===" << std::endl;
    std::cout << "1. Automatic container detection (Docker, Podman, LXC)" << std::endl;
    std::cout << "2. Automatic environment setup" << std::endl;
    std::cout << "3. Simplified lifecycle management" << std::endl;
    std::cout << "4. Reduced boilerplate code in tests" << std::endl;
    std::cout << "5. Consistent timeout handling (5 minutes default)" << std::endl;
    std::cout << "6. Automatic logging to stderr" << std::endl;
    std::cout << "7. Automatic cleanup on completion" << std::endl;

    return exit_code == 0 ? 0 : 1;
}

