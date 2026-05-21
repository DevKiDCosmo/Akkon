#include "TestFramework.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <thread>
#include <fcntl.h>
#include <cerrno>

namespace testsuite {

TestFramework::TestFramework() = default;

TestFramework::~TestFramework() {
    cleanup();
}

std::string TestFramework::generateTrackerId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string TestFramework::getBinaryPath() const {
    // First, check environment variable (set by CMake/CTest)
    const char* env_binary = std::getenv("AKKON_BINARY");
    if (env_binary && std::filesystem::exists(env_binary)) {
        return env_binary;
    }

    // Try to find the Akkon binary in common build directories
    std::vector<std::string> possible_paths = {
        "./Akkon",
        "./cmake-build-debug/Akkon",
        "../cmake-build-debug/Akkon",
        "../../cmake-build-debug/Akkon",
        "../../../cmake-build-debug/Akkon"
    };

    for (const auto& path : possible_paths) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).string();
        }
    }

    // Last resort: assume it's in PATH
    return "Akkon";
}

std::string TestFramework::createInstance(const std::vector<std::string>& args, const std::string& log_dir) {
    std::string tracker_id = generateTrackerId();

    std::filesystem::path logs_dir = log_dir.empty() ? "test_logs" : log_dir;
    try {
        std::filesystem::create_directories(logs_dir);
    } catch (const std::exception& e) {
        std::cerr << "[TestFramework::createInstance] Failed to create log directory: " << e.what() << std::endl;
        return "";
    }

    InstanceInfo info;
    info.tracker_id = tracker_id;
    info.binary_path = getBinaryPath();
    info.args = args;
    info.start_time = std::chrono::system_clock::now();
    info.log_file = (logs_dir / (tracker_id + ".log")).string();
    info.status = "starting";

    std::cerr << "[TestFramework::createInstance] Binary path: " << info.binary_path << std::endl;
    std::cerr << "[TestFramework::createInstance] Log file: " << info.log_file << std::endl;

    m_instances[tracker_id] = info;

    if (!startProcess(tracker_id, args)) {
        info.status = "error";
        m_instances[tracker_id] = info;
        std::cerr << "[TestFramework::createInstance] startProcess failed" << std::endl;
        return "";
    }

    m_instances[tracker_id].status = "running";

    return tracker_id;
}

bool TestFramework::startProcess(const std::string& tracker_id, const std::vector<std::string>& args) {
    auto it = m_instances.find(tracker_id);
    if (it == m_instances.end()) {
        return false;
    }

    InstanceInfo& info = it->second;

    // Open log file
    std::ofstream log_file(info.log_file, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "[TestFramework] Failed to open log file: " << info.log_file << std::endl;
        return false;
    }

    // Debug: check if binary exists
    if (!std::filesystem::exists(info.binary_path)) {
        log_file << "[ERROR] Binary not found: " << info.binary_path << std::endl;
        std::cerr << "[TestFramework] BINARY NOT FOUND: " << info.binary_path << std::endl;
        std::cerr << "[TestFramework] CWD: " << std::filesystem::current_path().string() << std::endl;
        log_file.close();
        return false;
    }

    log_file << "[" << tracker_id << "] Starting process: " << info.binary_path << std::endl;
    for (const auto& arg : args) {
        log_file << "  arg: " << arg << std::endl;
    }
    log_file.flush();
    log_file.close();

    // Fork and exec
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "[TestFramework] fork() failed" << std::endl;
        return false;
    }

    if (pid == 0) {
        // Child process

        // Redirect stdout/stderr to log file
        int fd = open(info.log_file.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd != -1) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(info.binary_path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // Execute
        execv(info.binary_path.c_str(), argv.data());

        // If we get here, exec failed
        std::cerr << "exec failed: " << strerror(errno) << std::endl;
        exit(127);
    } else {
        // Parent process - update PID in the instance map
        it->second.pid = pid;

        // Write to log that process started
        std::ofstream log_append(info.log_file, std::ios::app);
        log_append << "[" << tracker_id << "] Process started with PID " << pid << std::endl;
        log_append.flush();
    }

    return true;
}

InstanceInfo TestFramework::getTracking(const std::string& tracker_id) const {
    const auto it = m_instances.find(tracker_id);
    if (it != m_instances.end()) {
        return it->second;
    }
    return InstanceInfo{};
}

InstanceStatus TestFramework::getStatus(const std::string& tracker_id) const {
    const auto it = m_instances.find(tracker_id);
    if (it == m_instances.end()) {
        return InstanceStatus{-1, "", "not_found", 0, "", -1};
    }

    const auto& info = it->second;
    InstanceStatus status;
    status.pid = info.pid;
    status.tracker_id = info.tracker_id;
    status.status = info.status;

    // Calculate uptime
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.start_time);
    status.uptime_ms = uptime.count();

    // Get last log line
    std::ifstream log(info.log_file);
    std::string line;
    if (log.is_open()) {
        while (std::getline(log, line)) {
            status.last_log_line = line;
        }
    }

    // Check if process is still running
    if (info.pid != -1) {
        int wstatus = 0;
        pid_t result = waitpid(info.pid, &wstatus, WNOHANG);
        if (result == info.pid) {
            if (WIFEXITED(wstatus)) {
                status.exit_code = WEXITSTATUS(wstatus);
            } else if (WIFSIGNALED(wstatus)) {
                status.exit_code = -WTERMSIG(wstatus);
            }
            status.status = "stopped";
            const_cast<TestFramework*>(this)->m_instances[tracker_id].status = "stopped";
        } else if (result == -1 && errno == ECHILD) {
            status.status = "stopped";
            const_cast<TestFramework*>(this)->m_instances[tracker_id].status = "stopped";
        }
    }

    return status;
}

std::string TestFramework::getLogPath(const std::string& tracker_id) const {
    const auto info = getTracking(tracker_id);
    return info.log_file.empty() ? "" : info.log_file;
}

std::vector<std::string> TestFramework::getLogs(const std::string& tracker_id, std::size_t num_lines) const {
    std::vector<std::string> lines;
    const auto info = getTracking(tracker_id);

    if (info.log_file.empty()) {
        return lines;
    }

    std::ifstream log(info.log_file);
    std::string line;

    if (num_lines == 0) {
        // Get all lines
        while (std::getline(log, line)) {
            lines.push_back(line);
        }
    } else {
        // Get last num_lines
        std::vector<std::string> all_lines;
        while (std::getline(log, line)) {
            all_lines.push_back(line);
        }

        if (all_lines.size() <= num_lines) {
            lines = all_lines;
        } else {
            lines.assign(all_lines.end() - num_lines, all_lines.end());
        }
    }

    return lines;
}

bool TestFramework::stopInstance(const std::string& tracker_id) {
    const auto it = m_instances.find(tracker_id);
    if (it == m_instances.end() || it->second.pid == -1) {
        return false;
    }

    int pid = it->second.pid;
    int wstatus = 0;
    if (waitpid(pid, &wstatus, WNOHANG) == pid) {
        it->second.status = "stopped";
        return true;
    }

    if (kill(pid, SIGTERM) != 0) {
        if (errno == ESRCH) {
            it->second.status = "stopped";
            return true;
        }
        return false;
    }

    // Give it 2 seconds to terminate gracefully
    for (int i = 0; i < 20; i++) {
        pid_t res = waitpid(pid, &wstatus, WNOHANG);
        if (res == pid || (res == -1 && errno == ECHILD)) {
            // Process is gone
            it->second.status = "stopped";
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

bool TestFramework::killInstance(const std::string& tracker_id) {
    const auto it = m_instances.find(tracker_id);
    if (it == m_instances.end() || it->second.pid == -1) {
        return false;
    }

    int pid = it->second.pid;
    kill(pid, SIGKILL);
    
    int wstatus = 0;
    waitpid(pid, &wstatus, 0); // reap the zombie synchronously

    it->second.status = "stopped";
    return true;
}

bool TestFramework::waitForInstance(const std::string& tracker_id, std::int32_t timeout_ms) {
    const auto start = std::chrono::steady_clock::now();

    while (true) {
        if (!isRunning(tracker_id)) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();

        if (elapsed >= timeout_ms) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

std::vector<std::string> TestFramework::listInstances() const {
    std::vector<std::string> ids;
    for (const auto& [id, _] : m_instances) {
        ids.push_back(id);
    }
    return ids;
}

bool TestFramework::isRunning(const std::string& tracker_id) const {
    const auto status = getStatus(tracker_id);
    return status.status == "running";
}

std::int32_t TestFramework::getExitCode(const std::string& tracker_id) const {
    const auto status = getStatus(tracker_id);
    return status.exit_code;
}

void TestFramework::cleanup() {
    for (auto& [tracker_id, info] : m_instances) {
        if (isRunning(tracker_id)) {
            killInstance(tracker_id);
        }
    }
    m_instances.clear();
}

} // namespace testsuite


