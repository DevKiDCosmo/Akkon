#pragma once
#include "IArgumentParser.h"

namespace shell {

class ArgumentParser final : public IArgumentParser {
public:
    bool parse(int argc, char* argv[], ShellOptions& options, std::string& error) override;
};

} // namespace shell

