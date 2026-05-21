#include "memory/ArenaAllocator.h"
#include "security/LifecycleManager.h"
#include <iostream>
#include <cassert>
#include <stdexcept>

int main() {
    std::cout << "=== Akkon Memory Safety & Alarm Test ===" << std::endl;

    // Initialize lockdown state
    security::LifecycleManager::triggerLockdown(security::LockdownState::NONE);
    assert(security::LifecycleManager::getLockdownState() == security::LockdownState::NONE);

    // 1. Basic Allocation & Verification
    memory::ArenaAllocator arena(1024);
    void* p1 = arena.allocate(100);
    assert(p1 != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(p1) % 64 == 0); // Aligned

    // Verify should pass initially
    try {
        arena.verify();
        std::cout << "  [PASS] Initial verification succeeded." << std::endl;
    } catch (...) {
        assert(false && "verify() threw unexpectedly on intact memory");
    }

    // 2. Out-of-bounds check_access alarm
    std::cout << "Testing check_access boundary alarm..." << std::endl;
    bool boundary_threw = false;
    try {
        // Access address past the end of the allocator's buffer (p1 + 2000 is way outside)
        const char* bad_ptr = static_cast<const char*>(p1) + 2000;
        arena.check_access(bad_ptr, 10);
    } catch (const std::runtime_error& e) {
        boundary_threw = true;
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    assert(boundary_threw && "check_access failed to throw on out-of-bounds access");
    assert(security::LifecycleManager::getLockdownState() == security::LockdownState::IMMEDIATE && "check_access failed to trigger IMMEDIATE lockdown");

    // Reset lockdown state for the next test
    security::LifecycleManager::triggerLockdown(security::LockdownState::NONE);

    // 3. Canary corruption verification alarm
    std::cout << "Testing canary corruption verification alarm..." << std::endl;
    
    // Corrupt the footer canary of the allocation (which is located at user_ptr + size)
    char* corrupt_ptr = static_cast<char*>(p1) + 100; // start of footer canary
    *corrupt_ptr = 0x00; // corrupt first byte

    bool verify_threw = false;
    try {
        arena.verify();
    } catch (const std::runtime_error& e) {
        verify_threw = true;
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    assert(verify_threw && "verify() failed to throw on corrupted canary");
    assert(security::LifecycleManager::getLockdownState() == security::LockdownState::IMMEDIATE && "verify() failed to trigger IMMEDIATE lockdown");

    std::cout << "=== All memory safety and alarm tests passed! ===" << std::endl;
    return 0;
}
