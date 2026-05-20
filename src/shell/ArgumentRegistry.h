#pragma once
#include "../memory/RuntimeDomain.h"
#include <string>
#include <vector>

namespace shell {

struct ArgumentMetadata {
    std::string name;       // "-c" or "--no-import"
    bool takes_arg = false; // if true, expects a value after the argument
    std::string format;     // e.g., "integer", "string", "flag"
    std::string help;       // help text
    memory::RuntimeDomain domain = memory::RuntimeDomain::RUNTIME;
};

struct ParseResult {
    bool success = false;
    std::vector<std::string> parsed_args;      // successfully parsed args with values
    std::vector<std::string> unregistered;     // unregistered args (if debug mode)
    std::vector<std::string> warnings;         // warnings from parsing
    std::string error;                         // error message if success == false
};

class ArgumentRegistry {
public:
    static ArgumentRegistry& instance();

    void registerArgument(const ArgumentMetadata& meta);
    ParseResult parse(int argc, char* argv[], bool debug_mode = false);
    void printRegistered() const;

private:
    ArgumentRegistry() = default;
    std::vector<ArgumentMetadata> m_registry;
};

} // namespace shell

