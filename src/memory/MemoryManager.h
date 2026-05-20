#pragma once
#include "RuntimeDomain.h"
#include "ArenaAllocator.h"
#include <map>
#include <memory>

namespace memory {

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    ArenaAllocator* getDomain(RuntimeDomain domain);
    void resetDomain(RuntimeDomain domain);
    void printStats() const;

private:
    std::map<RuntimeDomain, std::unique_ptr<ArenaAllocator>> m_domains;
};

} // namespace memory
