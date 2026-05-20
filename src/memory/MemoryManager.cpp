#include "MemoryManager.h"
#include <iostream>

namespace memory {

MemoryManager::MemoryManager() {
    m_domains[RuntimeDomain::RUNTIME_META] = std::make_unique<Arena>(10 * 1024 * 1024); // 10 MB
    m_domains[RuntimeDomain::RUNTIME_REQUEST] = std::make_unique<Arena>(5 * 1024 * 1024);  // 5 MB
}

MemoryManager::~MemoryManager() = default;

Arena* MemoryManager::getDomain(RuntimeDomain domain) {
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
        const char* name = (domain == RuntimeDomain::RUNTIME_META) ? "RUNTIME_META" : "RUNTIME_REQUEST";
        std::cout << name << ": " << arena->allocated() << " / " << arena->capacity() << " bytes"
                  << " (" << arena->allocation_count() << " allocations)" << std::endl;
    }
}

} // namespace memory

