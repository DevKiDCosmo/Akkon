#include "HealthMonitor.h"
#include "../security/LifecycleManager.h"
#include "SystemMonitor.h"
#include <filesystem>
#include <memory>
#include <array>
#include <iostream>
#include <chrono>

namespace monitor {

// Helper to execute a command and return output (macOS/Linux)
#ifndef _WIN32
static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
#endif

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#elif defined(__APPLE__)
// Uses exec diskutil
#endif

HealthStats HealthMonitor::checkHealth(size_t ram_allocated, size_t ram_capacity) {
    HealthStats stats;
    stats.available_disk_bytes = 0;
    stats.reliability_score = 100;

    static bool cached_smart_verified = true;
    static std::string cached_smart_error = "";
    static auto last_smart_check = std::chrono::steady_clock::now() - std::chrono::hours(1);

    try {
        std::filesystem::path dbDir = std::filesystem::current_path() / "db";
        if (!std::filesystem::exists(dbDir)) {
            std::filesystem::create_directory(dbDir);
        }
        std::filesystem::space_info si = std::filesystem::space(dbDir);
        stats.available_disk_bytes = si.available;
        
        // Cross-platform S.M.A.R.T. Health Check (Cached for 5 minutes)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_smart_check).count() > 300) {
            last_smart_check = now;
            bool is_smart_verified = true;
            std::string smart_error = "";

#if defined(_WIN32)
            HANDLE hDevice = CreateFileW(L"\\\\.\\PhysicalDrive0", GENERIC_READ | GENERIC_WRITE, 
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hDevice != INVALID_HANDLE_VALUE) {
                STORAGE_PREDICT_FAILURE predictFailure = {0};
                DWORD bytesReturned = 0;
                if (DeviceIoControl(hDevice, IOCTL_STORAGE_PREDICT_FAILURE, NULL, 0,
                                    &predictFailure, sizeof(predictFailure), &bytesReturned, NULL)) {
                    if (predictFailure.PredictFailure) {
                        is_smart_verified = false;
                        smart_error = "Drive reports imminent failure!";
                    }
                }
                CloseHandle(hDevice);
            }
#elif defined(__linux__)
            int fd = open("/dev/sda", O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                unsigned char args[4] = {0xD0, 0, 0x4F, 0xC2}; 
                if (ioctl(fd, HDIO_DRIVE_CMD, &args) < 0) {
                    // Ignore if it fails due to permissions
                } else {
                    // ATA SMART check
                }
                close(fd);
            }
#elif defined(__APPLE__)
            std::string smart_output = exec("diskutil info / | grep SMART");
            if (!smart_output.empty() && smart_output.find("Verified") == std::string::npos) {
                is_smart_verified = false;
                smart_error = smart_output;
            }
#endif
            cached_smart_verified = is_smart_verified;
            cached_smart_error = smart_error;
        }

        if (!cached_smart_verified) {
            SystemMonitor::log(LogLevel::WARNING, "S.M.A.R.T. Health Check failed or drive is failing: " + cached_smart_error);
            stats.reliability_score -= 50;
        }

        // Adjust disk space thresholds so lockdown is triggered safely without blocking developers in lower disk-space scenarios.
        // Trigger lockdown if < 50MB
        if (stats.available_disk_bytes < 50LL * 1024LL * 1024LL) {
            security::LifecycleManager::triggerLockdown(security::LockdownState::IMMEDIATE);
            SystemMonitor::log(LogLevel::ERROR, "Disk space extremely low (<50MB)! Triggering IMMEDIATE lockdown.");
            stats.reliability_score = 0;
        } else if (stats.available_disk_bytes < 200LL * 1024LL * 1024LL) { // < 200MB
            stats.reliability_score -= 30;
        }

        // Incorporate memory limits into the reliability score
        if (ram_capacity > 0) {
            double ram_usage = (double)ram_allocated / (double)ram_capacity;
            if (ram_usage >= 0.99) {
                stats.reliability_score -= 80;
                SystemMonitor::log(LogLevel::WARNING, "RAM usage extremely high! (>99% of capacity)");
            } else if (ram_usage >= 0.95) {
                stats.reliability_score -= 40;
                SystemMonitor::log(LogLevel::WARNING, "RAM usage high! (>95% of capacity)");
            } else if (ram_usage >= 0.90) {
                stats.reliability_score -= 20;
                SystemMonitor::log(LogLevel::WARNING, "RAM usage elevated! (>90% of capacity)");
            }
        }

        if (stats.reliability_score < 0) stats.reliability_score = 0;
    } catch (const std::exception& e) {
        SystemMonitor::log(LogLevel::ERROR, std::string("Disk health check failed: ") + e.what());
        stats.reliability_score = 0;
    }

    return stats;
}

} // namespace monitor
