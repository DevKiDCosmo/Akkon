#include "MemoryManager.h"
#include <iostream>

namespace memory {

MemoryManager::MemoryManager() {
    m_domains[RuntimeDomain::RUNTIME]      = std::make_unique<ArenaAllocator>(10 * 1024 * 1024); // 10 MB
    m_domains[RuntimeDomain::REQUEST]      = std::make_unique<ArenaAllocator>(5 * 1024 * 1024);  // 5 MB
    m_domains[RuntimeDomain::RESERVED]     = std::make_unique<ArenaAllocator>(1 * 1024 * 1024);  // 1 MB
    m_domains[RuntimeDomain::VERIFICATION] = std::make_unique<ArenaAllocator>(2 * 1024 * 1024);  // 2 MB
    m_domains[RuntimeDomain::LIFECYCLE]    = std::make_unique<ArenaAllocator>(2 * 1024 * 1024);  // 2 MB
}

MemoryManager::~MemoryManager() = default;

ArenaAllocator* MemoryManager::getDomain(RuntimeDomain domain) {
    auto it = m_domains.find(domain);
    return it != m_domains.end() ? it->second.get() : nullptr;
}

void MemoryManager::resetDomain(RuntimeDomain domain) {
    auto it = m_domains.find(domain);
    if (it != m_domains.end()) {
        it->second->reset();
    }
}

void MemoryManager::printStats() const {
    std::cout << "\n=== Memory Manager Stats ===" << std::endl;
    for (const auto& [domain, arena] : m_domains) {
        std::string name;
        switch(domain) {
            case RuntimeDomain::RUNTIME: name = "RUNTIME"; break;
            case RuntimeDomain::REQUEST: name = "REQUEST"; break;
            case RuntimeDomain::RESERVED: name = "RESERVED"; break;
            case RuntimeDomain::VERIFICATION: name = "VERIFICATION"; break;
            case RuntimeDomain::LIFECYCLE: name = "LIFECYCLE"; break;
        }
        std::cout << name << ": " << arena->allocated() << " / " << arena->capacity() << " bytes"
                  << " (" << arena->allocation_count() << " allocations)" << std::endl;
    }
}

} // namespace memory

