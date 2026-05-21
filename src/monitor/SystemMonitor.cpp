#include "SystemMonitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#define io_write write
#else
#include <io.h>
#define io_write _write
#endif

namespace monitor {

int SystemMonitor::s_write_fd = -1;
bool SystemMonitor::s_debug_mode = false;
SystemMonitor::LogCallback SystemMonitor::s_callback = nullptr;
std::mutex SystemMonitor::s_callback_mutex;

void SystemMonitor::setLogCallback(LogCallback callback) {
    std::lock_guard<std::mutex> lock(s_callback_mutex);
    s_callback = callback;
}

void SystemMonitor::setDebugMode(bool debug_mode) {
    s_debug_mode = debug_mode;
}

bool SystemMonitor::init(bool debug_mode) {
    s_debug_mode = debug_mode;
#ifndef _WIN32
    int pipefd[2];
    if (pipe(pipefd) == -1) return true;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return true;
    }

    if (pid > 0) {
        close(pipefd[0]);
        s_write_fd = pipefd[1];
        return true;
    } else {
        close(pipefd[1]);
        runWatchdog(pipefd[0], getppid());
        exit(0);
        return false;
    }
#else
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
        return true;
    }
    if (!SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return true;
    }

    char szCmdline[MAX_PATH];
    GetModuleFileNameA(NULL, szCmdline, MAX_PATH);
    
    std::string cmd = std::string(szCmdline) + " --watchdog " + std::to_string((unsigned long long)hRead) + " " + std::to_string(GetCurrentProcessId());
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return true;
    }

    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    s_write_fd = _open_osfhandle((intptr_t)hWrite, _O_TEXT);
    return true;
#endif
}

void SystemMonitor::log(LogLevel level, const std::string& message, const std::string& tracker_id) {
    if (!s_debug_mode && level != LogLevel::ERROR) return;

    std::string level_str;
    switch (level) {
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARNING: level_str = "WARNING"; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
    }
    
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    char time_buf[26];
#ifndef _WIN32
    ctime_r(&now_time, time_buf);
#else
    ctime_s(time_buf, sizeof(time_buf), &now_time);
#endif
    time_buf[24] = '\0'; // remove newline
    
    ss << "[" << time_buf << "] [" << level_str << "] [" << tracker_id << "] " << message << "\n";
    std::string formatted = ss.str();

    if (s_write_fd != -1) {
        io_write(s_write_fd, formatted.c_str(), formatted.size());
    } else {
        std::cout << formatted;
        std::cout.flush();
    }

    {
        std::lock_guard<std::mutex> lock(s_callback_mutex);
        if (s_callback) {
            s_callback(level, formatted);
        }
    }
}

#ifndef _WIN32
void SystemMonitor::runWatchdog(int log_read_fd, pid_t main_pid) {
    std::ofstream log_file("akkon_server.log", std::ios::app);
    log_file << "--- Watchdog Started (Monitoring PID " << main_pid << ") ---\n";
    log_file.flush();
    std::cout << "--- Watchdog Started (Monitoring PID " << main_pid << ") ---\n";
    std::cout.flush();

    pollfd pfd;
    pfd.fd = log_read_fd;
    pfd.events = POLLIN;

    bool main_alive = true;
    while (main_alive) {
        int ret = poll(&pfd, 1, 1000);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buffer[1024];
            ssize_t bytes = read(log_read_fd, buffer, sizeof(buffer) - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                log_file << buffer;
                log_file.flush();
                std::cout << buffer;
                std::cout.flush();
            } else if (bytes == 0) {
                main_alive = false;
            }
        }
        if (main_alive) {
            if (getppid() == 1 || (kill(main_pid, 0) == -1 && errno == ESRCH)) {
                log_file << "--- Main process exited ---\n";
                log_file.flush();
                std::cout << "--- Main process exited ---\n";
                std::cout.flush();
                main_alive = false;
            }
        }
    }
    log_file << "--- Watchdog Exiting ---\n";
    log_file.flush();
    std::cout << "--- Watchdog Exiting ---\n";
    std::cout.flush();
    close(log_read_fd);
}
#else
void SystemMonitor::runWatchdog(HANDLE log_read_pipe, DWORD main_pid) {
    std::ofstream log_file("akkon_server.log", std::ios::app);
    log_file << "--- Watchdog Started (Monitoring PID " << main_pid << ") ---\n";
    log_file.flush();
    std::cout << "--- Watchdog Started (Monitoring PID " << main_pid << ") ---\n";
    std::cout.flush();

    HANDLE hParent = OpenProcess(SYNCHRONIZE, FALSE, main_pid);
    char buffer[1024];
    DWORD bytesRead;
    
    while (true) {
        DWORD bytesAvail;
        if (PeekNamedPipe(log_read_pipe, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
            if (ReadFile(log_read_pipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                log_file << buffer;
                log_file.flush();
                std::cout << buffer;
                std::cout.flush();
            }
        }
        
        if (hParent != NULL) {
            if (WaitForSingleObject(hParent, 100) == WAIT_OBJECT_0) {
                log_file << "--- Main process exited ---\n";
                log_file.flush();
                std::cout << "--- Main process exited ---\n";
                std::cout.flush();
                break;
            }
        } else {
            break;
        }
    }
    
    if (hParent) CloseHandle(hParent);
    log_file << "--- Watchdog Exiting ---\n";
    log_file.flush();
    std::cout << "--- Watchdog Exiting ---\n";
    std::cout.flush();
}
#endif

} // namespace monitor
