#include "TestFramework.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <cerrno>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

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

    std::string bin_name = "Akkon";
#ifdef _WIN32
    bin_name = "Akkon.exe";
#endif

    // Try to find the Akkon binary in common build directories
    std::vector<std::string> possible_paths = {
        "./" + bin_name,
        "./cmake-build-debug/" + bin_name,
        "../cmake-build-debug/" + bin_name,
        "../../cmake-build-debug/" + bin_name,
        "../../../cmake-build-debug/" + bin_name
    };

    for (const auto& path : possible_paths) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).string();
        }
    }

    // Last resort: assume it's in PATH
    return bin_name;
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

#ifdef _WIN32
    // Windows process startup
    std::string cmd = "\"" + info.binary_path + "\"";
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hLog = CreateFileA(
        info.log_file.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hLog == INVALID_HANDLE_VALUE) {
        std::cerr << "[TestFramework] Windows CreateFileA failed for log file: " << GetLastError() << std::endl;
        return false;
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hLog;
    si.hStdError = hLog;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(cmd.c_str()),
        NULL,
        NULL,
        TRUE, // Inherit handles (crucial for redirecting stdout/stderr)
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(hLog);

    if (!success) {
        std::cerr << "[TestFramework] Windows CreateProcessA failed: " << GetLastError() << std::endl;
        return false;
    }

    info.pid = static_cast<std::int32_t>(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::ofstream log_append(info.log_file, std::ios::app);
    log_append << "[" << tracker_id << "] Process started on Windows with PID " << info.pid << std::endl;
    log_append.flush();

    return true;
#else
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
#endif
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
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, info.pid);
        if (hProcess == NULL) {
            status.status = "stopped";
            const_cast<TestFramework*>(this)->m_instances[tracker_id].status = "stopped";
        } else {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode)) {
                if (exitCode != STILL_ACTIVE) {
                    status.exit_code = static_cast<std::int32_t>(exitCode);
                    status.status = "stopped";
                    const_cast<TestFramework*>(this)->m_instances[tracker_id].status = "stopped";
                }
            }
            CloseHandle(hProcess);
        }
#else
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
#endif
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
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (hProcess == NULL) {
        it->second.status = "stopped";
        return true;
    }

    if (!TerminateProcess(hProcess, 0)) {
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hProcess, 2000);
    CloseHandle(hProcess);
    it->second.status = "stopped";
    return true;
#else
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
#endif
}

bool TestFramework::killInstance(const std::string& tracker_id) {
    const auto it = m_instances.find(tracker_id);
    if (it == m_instances.end() || it->second.pid == -1) {
        return false;
    }

    int pid = it->second.pid;
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (hProcess != NULL) {
        TerminateProcess(hProcess, 1);
        WaitForSingleObject(hProcess, INFINITE);
        CloseHandle(hProcess);
    }
#else
    kill(pid, SIGKILL);
    
    int wstatus = 0;
    waitpid(pid, &wstatus, 0); // reap the zombie synchronously
#endif

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

bool TestFramework::isContainerized() const {
    // Check for common containerization indicators
#ifndef _WIN32
    // Check for .dockerenv file
    if (std::filesystem::exists("/.dockerenv")) {
        return true;
    }

    // Check for cgroup v1
    std::ifstream cgroup1("/proc/self/cgroup");
    if (cgroup1.is_open()) {
        std::string line;
        while (std::getline(cgroup1, line)) {
            if (line.find("/docker") != std::string::npos ||
                line.find("/podman") != std::string::npos ||
                line.find("/lxc") != std::string::npos ||
                line.find("/kubepods") != std::string::npos) {
                return true;
            }
        }
    }

    // Check for cgroup v2
    std::ifstream cgroup2("/proc/self/cgroup");
    if (cgroup2.is_open()) {
        std::string line;
        while (std::getline(cgroup2, line)) {
            if (line.find("docker") != std::string::npos ||
                line.find("podman") != std::string::npos ||
                line.find("lxc") != std::string::npos ||
                line.find("kubepods") != std::string::npos) {
                return true;
            }
        }
    }
#endif
    return false;
}

std::string TestFramework::detectContainerEngine() const {
    // Try to detect which container engine is available/in use
#ifndef _WIN32
    // Check for Docker
    if (std::filesystem::exists("/var/run/docker.sock") ||
        std::getenv("DOCKER_HOST") != nullptr) {
        return "docker";
    }

    // Check for Podman
    if (std::filesystem::exists("/var/run/podman/podman.sock") ||
        std::getenv("PODMAN_HOST") != nullptr) {
        return "podman";
    }

    // Check for containerd
    if (std::filesystem::exists("/var/run/containerd/containerd.sock")) {
        return "containerd";
    }

    // Check for LXC
    if (std::filesystem::exists("/var/lib/lxc") ||
        std::filesystem::exists("/var/snap/lxd/common/lxd")) {
        return "lxc";
    }
#endif
    return "none";
}

void TestFramework::setupEnvironment(std::map<std::string, std::string>& env) const {
    // Set up environment based on container status and engine

    // Detect if we're in a container
    bool in_container = isContainerized();
    if (in_container) {
        env["AKKON_IN_CONTAINER"] = "1";
        std::string engine = detectContainerEngine();
        if (!engine.empty() && engine != "none") {
            env["AKKON_CONTAINER_ENGINE"] = engine;
        }
    }

    // Set standard test environment variables
    if (env.find("AKKON_BINARY") == env.end()) {
        env["AKKON_BINARY"] = getBinaryPath();
    }

    // Add test-specific environment
    env["AKKON_TEST_MODE"] = "1";

    // If not already set, add current working directory context
    if (env.find("AKKON_CWD") == env.end()) {
        env["AKKON_CWD"] = std::filesystem::current_path().string();
    }
}

std::int32_t TestFramework::runtime(const std::vector<std::string>& args,
                                     const std::map<std::string, std::string>& env) {
    // All-in-one runtime: setup, run, cleanup with automatic environment management

    // Setup environment with provided variables and detected settings
    std::map<std::string, std::string> runtime_env = env;
    setupEnvironment(runtime_env);

    // Apply environment variables to current process (for child process inheritance)
    for (const auto& [key, value] : runtime_env) {
        setenv(key.c_str(), value.c_str(), 1);
    }

    // Create instance with args
    std::string tracker_id = createInstance(args, "test_logs");
    if (tracker_id.empty()) {
        std::cerr << "[TestFramework::runtime] Failed to create instance" << std::endl;
        return -1;
    }

    std::cerr << "[TestFramework::runtime] Instance created with ID: " << tracker_id << std::endl;

    // Log environment information
    if (isContainerized()) {
        std::cerr << "[TestFramework::runtime] Running in containerized environment" << std::endl;
        std::cerr << "[TestFramework::runtime] Container engine: " << detectContainerEngine() << std::endl;
    } else {
        std::cerr << "[TestFramework::runtime] Running in native environment" << std::endl;
    }

    // Wait for instance to complete (default 5 minute timeout)
    const std::int32_t TIMEOUT_MS = 300000; // 5 minutes
    bool completed = waitForInstance(tracker_id, TIMEOUT_MS);

    if (!completed) {
        std::cerr << "[TestFramework::runtime] Instance timeout after " << TIMEOUT_MS << "ms" << std::endl;
        killInstance(tracker_id);
        return -1;
    }

    // Get exit code
    std::int32_t exit_code = getExitCode(tracker_id);

    // Log final status
    auto logs = getLogs(tracker_id, 5);
    std::cerr << "[TestFramework::runtime] Instance completed with exit code: " << exit_code << std::endl;
    if (!logs.empty()) {
        std::cerr << "[TestFramework::runtime] Last log entries:" << std::endl;
        for (const auto& line : logs) {
            std::cerr << "  " << line << std::endl;
        }
    }

    // Cleanup the instance
    stopInstance(tracker_id);

    return exit_code;
}

} // namespace testsuite


