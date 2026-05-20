#include "ArgumentRegistry.h"
#include <iostream>
#include <algorithm>

namespace shell {

ArgumentRegistry& ArgumentRegistry::instance() {
    static ArgumentRegistry inst;
    return inst;
}

void ArgumentRegistry::registerArgument(const ArgumentMetadata& meta) {
    if (std::any_of(m_registry.begin(), m_registry.end(),
                    [&meta](const auto& m) { return m.name == meta.name; })) {
        std::cerr << "[WARN] Argument " << meta.name << " already registered" << std::endl;
        return;
    }
    m_registry.push_back(meta);
}

ParseResult ArgumentRegistry::parse(int argc, char* argv[], bool debug_mode) {
    ParseResult result;
    result.success = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto it = std::find_if(m_registry.begin(), m_registry.end(),
                               [&arg](const auto& meta) { return meta.name == arg; });

        if (it == m_registry.end()) {
            if (debug_mode) {
                result.warnings.push_back("Unregistered argument: " + arg);
                result.unregistered.push_back(arg);
                continue;
            } else {
                result.success = false;
                result.error = "Unknown argument: " + arg;
                return result;
            }
        }

        if (it->takes_arg) {
            if (i + 1 >= argc) {
                result.success = false;
                result.error = "Argument " + arg + " requires a value";
                return result;
            }
            result.parsed_args.push_back(arg);
            result.parsed_args.push_back(argv[++i]);
        } else {
            result.parsed_args.push_back(arg);
        }
    }

    return result;
}

void ArgumentRegistry::printRegistered() const {
    std::cout << "=== Registered Arguments ===" << std::endl;
    for (const auto& meta : m_registry) {
        std::cout << meta.name;
        if (meta.takes_arg) std::cout << " <" << meta.format << ">";
        std::cout << " - " << meta.help << std::endl;
    }
}

} // namespace shell

