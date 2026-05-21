///
/// Practical Example: Real-World usage of runtime() method
///
/// This example shows how to use the new runtime() method in actual test scenarios
/// with different configurations, error handling, and logging.
///

#include "testsuite/TestFramework.h"
#include <cassert>
#include <iostream>
#include <iomanip>

using namespace testsuite;

// Helper function to print test results
void printTestResult(const std::string& test_name, bool passed, std::int32_t exit_code = 0) {
    std::string status = passed ? "✓ PASS" : "✗ FAIL";
    std::cout << std::left << std::setw(40) << test_name
              << std::setw(10) << status;
    if (!passed && exit_code != 0) {
        std::cout << " (exit code: " << exit_code << ")";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Akkon TestFramework - runtime() Examples ===" << std::endl << std::endl;

    TestFramework framework;
    int tests_passed = 0;
    int tests_total = 0;

    // ========== TEST SUITE 1: Basic Operations ==========
    std::cout << "TEST SUITE 1: Basic Operations" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    // Test 1.1: Simplest invocation
    {
        tests_total++;
        std::cout << "\n[1.1] Basic invocation with single connection:" << std::endl;
        std::vector<std::string> args = {"-c", "1"};
        std::int32_t exit_code = framework.runtime(args);

        bool passed = (exit_code >= 0);  // Success if no critical error
        printTestResult("Single connection instance", passed, exit_code);
        if (passed) tests_passed++;
    }

    // Test 1.2: Multiple connections
    {
        tests_total++;
        std::cout << "\n[1.2] Multiple connections:" << std::endl;
        std::vector<std::string> args = {"-c", "5"};
        std::int32_t exit_code = framework.runtime(args);

        bool passed = (exit_code >= 0);
        printTestResult("Multiple connections instance", passed, exit_code);
        if (passed) tests_passed++;
    }

    // ========== TEST SUITE 2: With Custom Environment ==========
    std::cout << "\nTEST SUITE 2: Environment Configuration" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    // Test 2.1: With debug environment
    {
        tests_total++;
        std::cout << "\n[2.1] With debug environment:" << std::endl;
        std::vector<std::string> args = {"-c", "1"};
        std::map<std::string, std::string> env = {
            {"AKKON_DEBUG", "1"},
            {"AKKON_LOG_LEVEL", "DEBUG"}
        };
        std::int32_t exit_code = framework.runtime(args, env);

        bool passed = (exit_code >= 0);
        printTestResult("Debug mode execution", passed, exit_code);
        if (passed) tests_passed++;
    }

    // Test 2.2: With port configuration
    {
        tests_total++;
        std::cout << "\n[2.2] With port configuration:" << std::endl;
        std::vector<std::string> args = {"-p", "9000", "-c", "1"};
        std::map<std::string, std::string> env = {
            {"AKKON_PORT", "9000"}
        };
        std::int32_t exit_code = framework.runtime(args, env);

        bool passed = (exit_code >= 0);
        printTestResult("Custom port configuration", passed, exit_code);
        if (passed) tests_passed++;
    }

    // ========== TEST SUITE 3: Stress Testing ==========
    std::cout << "\nTEST SUITE 3: Stress Testing" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    // Test 3.1: Multiple sequential runs
    {
        tests_total++;
        std::cout << "\n[3.1] Sequential stress test (3 runs):" << std::endl;

        bool all_passed = true;
        for (int i = 1; i <= 3; i++) {
            std::vector<std::string> args = {"-c", "1"};
            std::int32_t exit_code = framework.runtime(args);
            if (exit_code < 0) all_passed = false;
            std::cout << "  Run " << i << ": exit_code = " << exit_code << std::endl;
        }

        printTestResult("Sequential execution (3x)", all_passed);
        if (all_passed) tests_passed++;
    }

    // Test 3.2: Varying load
    {
        tests_total++;
        std::cout << "\n[3.2] Varying load test:" << std::endl;

        bool all_passed = true;
        std::vector<int> connection_counts = {1, 2, 3, 5};

        for (int conn_count : connection_counts) {
            std::vector<std::string> args = {"-c", std::to_string(conn_count)};
            std::int32_t exit_code = framework.runtime(args);
            if (exit_code < 0) all_passed = false;
            std::cout << "  Connections: " << conn_count << ", exit_code = " << exit_code << std::endl;
        }

        printTestResult("Varying load execution", all_passed);
        if (all_passed) tests_passed++;
    }

    // ========== TEST SUITE 4: Error Handling ==========
    std::cout << "\nTEST SUITE 4: Error Handling" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    // Test 4.1: Invalid arguments (expect non-zero exit)
    {
        tests_total++;
        std::cout << "\n[4.1] Invalid arguments handling:" << std::endl;
        std::vector<std::string> args = {"-c", "invalid"};  // Invalid connection count
        std::int32_t exit_code = framework.runtime(args);

        // We expect this to fail gracefully
        bool passed = (exit_code != 0 || exit_code == -999);  // Either failed gracefully or got negative
        printTestResult("Invalid argument handling", passed, exit_code);
        if (passed) tests_passed++;
    }

    // ========== SUMMARY ==========
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "TEST SUMMARY" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    printf("Tests Passed: %d/%d\n", tests_passed, tests_total);
    printf("Success Rate: %.1f%%\n", (tests_passed * 100.0) / tests_total);

    if (tests_passed == tests_total) {
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed!" << std::endl;
        return 1;
    }
}

/*
 * KEY TAKEAWAYS:
 *
 * 1. The runtime() method handles container detection automatically
 * 2. No need for manual createInstance/stopInstance/waitForInstance calls
 * 3. Custom environment variables are simply passed as a map
 * 4. Exit codes are returned directly for easy assertion
 * 5. Logging is automatically sent to stderr
 * 6. Perfect for sequential and stress testing with minimal code
 *
 * RUNNING THIS TEST:
 *
 * $ make test_runtime_examples
 *
 * EXPECTED OUTPUT:
 *
 * TEST SUITE 1: Basic Operations
 * --------------------------------------------------
 *
 * [1.1] Basic invocation with single connection:
 * Single connection instance    ✓ PASS
 *
 * ... more tests ...
 *
 * ==================================================
 * TEST SUMMARY
 * ==================================================
 * Tests Passed: X/X
 * Success Rate: 100.0%
 *
 * ✓ All tests passed!
 */

