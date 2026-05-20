#include "Shell.h"
#include "ArgumentRegistry.h"
#include "../construction/Utils.h"
#include "../construction/MasterDB.h"
#include <iostream>
#include <map>

using construction::MasterDB;
using construction::ensureDbDirectory;
using memory::RuntimeDomain;

namespace shell {

void Shell::printUsage() {
    std::cout << "Usage: Akkon [-c N] [--no-import] [--debug] [--help]\n"
              << "  -c N         Create or require N mapped databases\n"
              << "  --no-import  Skip importing unmapped .db files from db/\n"
              << "  --debug      Enable debug mode (warn on unregistered args)\n"
              << "  --help       Show this message\n";
}

void Shell::registerAllArguments() {
    auto& reg = ArgumentRegistry::instance();
    reg.registerArgument({"--help", false, "flag", "Show help", RuntimeDomain::RUNTIME_META});
    reg.registerArgument({"-c", true, "integer", "Number of databases", RuntimeDomain::RUNTIME_META});
    reg.registerArgument({"--no-import", false, "flag", "Skip importing unmapped DBs", RuntimeDomain::RUNTIME_META});
    reg.registerArgument({"--debug", false, "flag", "Enable debug mode", RuntimeDomain::RUNTIME_META});
}

int Shell::run(int argc, char* argv[]) {
    // Register all known arguments
    registerAllArguments();

    // Parse arguments using registry
    auto& reg = ArgumentRegistry::instance();
    auto parseResult = reg.parse(argc, argv, m_debug);

    // Log warnings on unregistered args in debug mode
    if (!parseResult.warnings.empty()) {
        for (const auto& warn : parseResult.warnings) {
            std::cout << "[WARN] " << warn << std::endl;
        }
    }

    if (!parseResult.success) {
        std::cerr << "[ERROR] " << parseResult.error << std::endl;
        printUsage();
        return 1;
    }

    // Process parsed arguments
    int numDatabases = 1;
    bool noImport = false;
    bool showHelp = false;

    for (std::size_t i = 0; i < parseResult.parsed_args.size(); ++i) {
        const auto& arg = parseResult.parsed_args[i];
        if (arg == "--help") {
            showHelp = true;
        } else if (arg == "-c" && i + 1 < parseResult.parsed_args.size()) {
            try {
                numDatabases = std::stoi(parseResult.parsed_args[++i]);
            } catch (...) {
                std::cerr << "[ERROR] Invalid value for -c" << std::endl;
                return 1;
            }
        } else if (arg == "--no-import") {
            noImport = true;
        } else if (arg == "--debug") {
            m_debug = true;
        }
    }

    if (showHelp) {
        printUsage();
        return 0;
    }

    auto dbDir = ensureDbDirectory();
    std::filesystem::path masterPath = dbDir / "master.db";
    if (const bool masterExists = std::filesystem::exists(masterPath); masterExists)
        std::cout << "[INFO] Found existing master database at: " << masterPath << std::endl;
    else
        std::cout << "[INFO] Creating new master database at: " << masterPath << std::endl;

    MasterDB master(dbDir);
    if (!master.isOpen()) {
        std::cerr << "[ERROR] Failed to open master DB" << std::endl;
        return 1;
    }

    if (!noImport) {
        master.scanAndImportUnmapped();
    } else {
        std::cout << "[INFO] Skipping unmapped import due to --no-import" << std::endl;
    }

    std::map<int, DatabaseInfo> mapExisting = master.loadExisting();
    std::cout << "[INFO] Loaded " << mapExisting.size() << " database(s) from master DB" << std::endl;

    if (auto missing = master.getMissing(mapExisting); !missing.empty()) {
        std::cout << "[INFO] Recreating " << missing.size() << " missing database(s)" << std::endl;
        master.recreateMissing(missing);
    }

    if (auto existingCount = static_cast<int>(mapExisting.size()); numDatabases > existingCount) {
        std::cout << "[INFO] Adding " << (numDatabases - existingCount) << " new database(s)..." << std::endl;
        for (int i = existingCount + 1; i <= numDatabases; ++i) {
            auto info = master.createNewDatabase();
            mapExisting[i] = info;
            std::cout << "[INFO] Created database " << i << ": " << info.uuid << " at " << info.filePos << std::endl;
        }
    } else if (numDatabases < existingCount) {
        std::cout << "[WARN] -c " << numDatabases << " specified, but " << existingCount
                  << " database(s) already exist. Keeping existing." << std::endl;
        std::cout << "[INFO] To reset remove: " << masterPath << std::endl;
    }

    std::cout << "\n=== Database Map ===" << std::endl;
    for (const auto& [idx, info] : mapExisting) {
        std::cout << "Database #" << idx << ": UUID=" << info.uuid
                  << " | status=" << info.status
                  << " | created=" << info.creationDate
                  << " | path=" << info.filePos << std::endl;
    }

    std::cout << "\n=== Master Database Registry ===" << std::endl;
    auto registry = master.loadExisting();
    for (const auto& [idx, info] : registry) {
        std::cout << "Entry #" << idx << ": " << info.uuid
                  << " | status=" << info.status
                  << " | " << info.creationDate
                  << " | " << info.filePos << std::endl;
    }

    std::cout << "\n[INFO] All databases are stored in: " << dbDir << std::endl;

    // Print memory stats if debug mode
    if (m_debug) {
        m_memMgr.printStats();
    }

    std::cout << std::endl;
    return 0;
}

} // namespace shell
