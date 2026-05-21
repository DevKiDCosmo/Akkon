#pragma once
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#else
#include <windows.h>
#endif

namespace monitor {

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG
};

class SystemMonitor {
public:
    static bool init(bool debug_mode = false);
    static void log(LogLevel level, const std::string& message);
    static void setDebugMode(bool debug_mode);
    static bool isDebugMode() { return s_debug_mode; }
    
#ifndef _WIN32
    static void runWatchdog(int log_read_fd, pid_t main_pid);
#else
    static void runWatchdog(HANDLE log_read_pipe, DWORD main_pid);
#endif

private:
    static int s_write_fd;
    static bool s_debug_mode;
};

} // namespace monitor
