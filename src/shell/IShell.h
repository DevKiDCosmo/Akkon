#pragma once

namespace shell {

class IShell {
public:
    virtual ~IShell() = default;
    virtual int run(int argc, char* argv[]) = 0;
};

} // namespace shell

