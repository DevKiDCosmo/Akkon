#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <memory>
#include <cstdint>

namespace testsuite {

/// Instance tracking info for a running Akkon process
struct InstanceInfo {
    std::int32_t pid = -1;           // Process ID
    std::string tracker_id;          // Unique tracker ID (UUID)
    std::string log_file;            // Path to log file
    std::chrono::system_clock::time_point start_time;
    std::string status;              // "running", "stopped", "error"
    std::string binary_path;         // Path to Akkon binary
    std::vector<std::string> args;   // Command line arguments passed
};

/// Status snapshot of a running instance
struct InstanceStatus {
    std::int32_t pid;
    std::string tracker_id;
    std::string status;
    std::uint64_t uptime_ms;         // Milliseconds since start
    std::string last_log_line;       // Last line from log file
    std::int32_t exit_code = -1;     // Only valid if status == "stopped"
};

/// Akkon Testing Framework
/// Manages multiple Akkon instances, tracking, logging, and status
class TestFramework {
public:
    TestFramework();
    ~TestFramework();

    /// Create and start a new Akkon instance
    /// Returns tracker_id on success, empty string on failure
    std::string createInstance(const std::vector<std::string>& args, const std::string& log_dir = "");

    /// Get tracking info for an instance
    InstanceInfo getTracking(const std::string& tracker_id) const;

    /// Get current status of an instance
    InstanceStatus getStatus(const std::string& tracker_id) const;

    /// Get log file path for an instance
    std::string getLogPath(const std::string& tracker_id) const;

    /// Read logs from an instance
    std::vector<std::string> getLogs(const std::string& tracker_id, std::size_t num_lines = 0) const;

    /// Stop an instance gracefully
    bool stopInstance(const std::string& tracker_id);

    /// Kill an instance forcefully
    bool killInstance(const std::string& tracker_id);

    /// Wait for an instance to finish (with timeout)
    bool waitForInstance(const std::string& tracker_id, std::int32_t timeout_ms = 5000);

    /// List all tracked instances
    std::vector<std::string> listInstances() const;

    /// Check if instance is running
    bool isRunning(const std::string& tracker_id) const;

    /// Get exit code (only valid after instance stops)
    std::int32_t getExitCode(const std::string& tracker_id) const;

    /// Cleanup all instances and logs (for teardown)
    void cleanup();

private:
    std::map<std::string, InstanceInfo> m_instances;
    std::string generateTrackerId() const;
    std::string getBinaryPath() const;
    bool startProcess(const std::string& tracker_id, const std::vector<std::string>& args);
};

} // namespace testsuite


