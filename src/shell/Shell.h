#pragma once
#include "IShell.h"
#include "../memory/MemoryManager.h"

namespace shell {

class Shell final : public IShell {
public:
    int run(int argc, char* argv[]) override;

private:
    memory::MemoryManager m_memMgr;
    bool m_debug = false;

    static void printUsage();
    void registerAllArguments();
};

} // namespace shell

