#include <iostream>
#include <filesystem>
#include <cassert>
#include <string>
#include "network/Server.h"
#include "construction/Utils.h"

int main() {
    std::cout << "=== Akkon OS Compatibility Test ===" << std::endl;

    // 1. Verify socket type sizes and definitions
    std::cout << "\n[TEST 1] Sockets definitions verification..." << std::endl;
#ifdef _WIN32
    std::cout << "  Platform: Windows" << std::endl;
    assert(sizeof(network::socket_t) == sizeof(unsigned long long) || sizeof(network::socket_t) == sizeof(unsigned int));
    std::cout << "  [PASS] socket_t size matches Windows socket handle size." << std::endl;
#else
    std::cout << "  Platform: Unix-like (macOS / Linux)" << std::endl;
    assert(sizeof(network::socket_t) == sizeof(int));
    std::cout << "  [PASS] socket_t size matches standard POSIX fd size (int)." << std::endl;
#endif

    // 2. Verify path directory separator behavior
    std::cout << "\n[TEST 2] Path directory separator normalization..." << std::endl;
    std::filesystem::path p1("some_folder");
    std::filesystem::path p2("sub_folder");
    std::filesystem::path joined = p1 / p2;
    std::string joined_str = joined.generic_string(); // returns forward slash always
    std::cout << "  Joined generic path: " << joined_str << std::endl;
    assert(joined_str == "some_folder/sub_folder");
    
    std::string native_str = joined.make_preferred().string();
    std::cout << "  Native path preferred: " << native_str << std::endl;
#ifdef _WIN32
    assert(native_str.find('\\') != std::string::npos);
#else
    assert(native_str.find('/') != std::string::npos);
#endif
    std::cout << "  [PASS] Path directory separators behave correctly based on platform preferred style." << std::endl;

    // 3. Verify normalization of mixed separators
    std::cout << "\n[TEST 3] Normalization of mixed separators..." << std::endl;
    std::string test1 = construction::normalizePath("folder1\\folder2/folder3");
    std::cout << "  Mixed normalized: " << test1 << std::endl;
    assert(test1 == "folder1/folder2/folder3");
    
    std::string test2 = construction::normalizePath("a/b/../c");
    std::cout << "  Relative normalization: " << test2 << std::endl;
    assert(test2 == "a/c");
    
    std::string test3 = construction::normalizePath("a\\\\b\\\\c");
    std::cout << "  All backslashes: " << test3 << std::endl;
    assert(test3 == "a/b/c");
    
    std::string test4 = construction::normalizePath("C:\\Windows\\System32\\drivers/etc");
    std::cout << "  Windows path style: " << test4 << std::endl;
    assert(test4 == "C:/Windows/System32/drivers/etc");

    std::cout << "  [PASS] Mixed separators are properly normalized to uniform generic syntax cross-platform." << std::endl;

    // 4. Verify DB directory creation utility
    std::cout << "\n[TEST 4] ensureDbDirectory cross-platform path resolution..." << std::endl;
    std::filesystem::path db_path = construction::ensureDbDirectory();
    std::cout << "  Resolved DB directory: " << db_path.string() << std::endl;
    assert(std::filesystem::exists(db_path));
    std::cout << "  [PASS] db directory successfully verified at path: " << db_path.string() << std::endl;

    std::cout << "\n=== All OS Compatibility tests passed! ===" << std::endl;
    return 0;
}
