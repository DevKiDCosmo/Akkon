#pragma once
#include <atomic>
#include <thread>
#include <string>

namespace security {

enum class LockdownState {
    NONE,
    CLOSE,
    IMMEDIATE
};

class LifecycleManager {
public:
    static void start();
    static void stop();
    static LockdownState getLockdownState();
    static void triggerLockdown(LockdownState state); // Exposed for tests

private:
    static void backgroundTask();
    static std::string fetchVulnerabilityStatus();
    
    static std::atomic<bool> s_running;
    static std::atomic<LockdownState> s_lockdown_state;
    static std::thread s_worker;
};

} // namespace security
