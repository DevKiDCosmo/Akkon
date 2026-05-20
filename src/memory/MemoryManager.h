#pragma once
#include "RuntimeDomain.h"
#include "Arena.h"
#include <map>
#include <memory>

namespace memory {

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    Arena* getDomain(RuntimeDomain domain);
    void resetDomain(RuntimeDomain domain);
    void printStats() const;

private:
    std::map<RuntimeDomain, std::unique_ptr<Arena>> m_domains;
};

} // namespace memory

