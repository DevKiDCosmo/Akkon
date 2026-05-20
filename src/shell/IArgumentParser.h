#pragma once
#include <string>
#include "ShellOptions.h"

namespace shell {

class IArgumentParser {
public:
    virtual ~IArgumentParser() = default;
    virtual bool parse(int argc, char* argv[], ShellOptions& options, std::string& error) = 0;
};

} // namespace shell

