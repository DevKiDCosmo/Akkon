#include <iostream>
#include <map>
#include <filesystem>
#include "construction/DatabaseInfo.h"
#include "construction/Utils.h"
#include "construction/MasterDB.h"

using namespace construction;

int main(int argc, char* argv[]) {
    int numDatabases = 1;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-c" && i + 1 < argc) {
            try { numDatabases = std::stoi(argv[i + 1]); i++; }
            catch (const std::exception& e) { std::cerr << "[ERROR] Invalid argument for -c: " << e.what() << std::endl; return 1; }
        }
    }
    if (numDatabases <= 0) { std::cerr << "[ERROR] Number of databases must be positive" << std::endl; return 1; }

    auto dbDir = ensureDbDirectory();
    std::filesystem::path masterPath = dbDir / "master.db";
    bool masterExists = std::filesystem::exists(masterPath);
    if (masterExists) std::cout << "[INFO] Found existing master database at: " << masterPath << std::endl;
    else std::cout << "[INFO] Creating new master database at: " << masterPath << std::endl;

    MasterDB master(dbDir);
    if (!master.isOpen()) { std::cerr << "[ERROR] Failed to open master DB" << std::endl; return 1; }

    // Import any unmapped DB files in db/ as external entries, then reload registry
    master.scanAndImportExternal();

    std::map<int, DatabaseInfo> mapExisting;
    mapExisting = master.loadExisting();
    if (masterExists) {
        std::cout << "[INFO] Loaded " << mapExisting.size() << " database(s) from master DB" << std::endl;
        auto missing = master.getMissing(mapExisting);
        if (!missing.empty()) { std::cout << "[INFO] Recreating " << missing.size() << " missing database(s)" << std::endl; master.recreateMissing(missing); }

        int existingCount = (int)mapExisting.size();
        if (numDatabases > existingCount) {
            std::cout << "[INFO] Adding " << (numDatabases - existingCount) << " new database(s)..." << std::endl;
            for (int i = existingCount + 1; i <= numDatabases; ++i) {
                auto info = master.createNewDatabase();
                mapExisting[i] = info;
                std::cout << "[INFO] Created database " << i << ": " << info.uuid << " at " << info.filePos << std::endl;
            }
        } else if (numDatabases < existingCount) {
            std::cout << "[WARN] -c " << numDatabases << " specified, but " << existingCount << " database(s) already exist. Keeping existing." << std::endl;
            std::cout << "[INFO] To reset remove: " << masterPath << std::endl;
        }
    } else {
        std::cout << "[INFO] Creating " << numDatabases << " new database(s)..." << std::endl;
        for (int i = 1; i <= numDatabases; ++i) {
            auto info = master.createNewDatabase();
            mapExisting[i] = info;
            std::cout << "[INFO] Created database " << i << ": " << info.uuid << " at " << info.filePos << std::endl;
        }
    }

    std::cout << "\n=== Database Map ===" << std::endl;
    for (const auto& [idx, info] : mapExisting) {
        std::cout << "Database #" << idx << ": UUID=" << info.uuid << " | created=" << info.creationDate << " | path=" << info.filePos << std::endl;
    }

    std::cout << "\n=== Master Database Registry ===" << std::endl;
    auto registry = master.loadExisting();
    for (const auto& [idx, info] : registry) {
        std::cout << "Entry #" << idx << ": " << info.uuid << " | " << info.creationDate << " | " << info.filePos << std::endl;
    }

    std::cout << "\n[INFO] All databases are stored in: " << dbDir << std::endl << std::endl;

    return 0;
}