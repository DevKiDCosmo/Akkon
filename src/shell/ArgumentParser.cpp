#include "ArgumentParser.h"
#include <string>

namespace shell {

bool ArgumentParser::parse(int argc, char* argv[], ShellOptions& options, std::string& error) {
    options = ShellOptions{};

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            options.help = true;
            return true;
        }

        if (arg == "--no-import") {
            options.noImport = true;
            continue;
        }

        if (arg == "-c") {
            if (i + 1 >= argc) {
                error = "Missing value for -c";
                return false;
            }
            try {
                options.count = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                error = "Invalid numeric value for -c";
                return false;
            }
            continue;
        }

        error = "Unknown argument: " + arg;
        return false;
    }

    if (options.count <= 0) {
        error = "Number of databases must be positive";
        return false;
    }

    return true;
}

} // namespace shell

