#include "Utils.h"
#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace construction {

std::string generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) ss << '-';
        else if (i == 14) ss << '4';
        else if (i == 19) ss << std::hex << ((dis(gen) & 0x3) | 0x8);
        else ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string escapeSqlString(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (c == '\'') result += "''";
        else result += c;
    }
    return result;
}

std::filesystem::path ensureDbDirectory() {
    std::filesystem::path dbDir = std::filesystem::current_path() / "db";
    if (!std::filesystem::exists(dbDir)) {
        std::filesystem::create_directory(dbDir);
        std::cout << "[INFO] Created DB directory: " << dbDir << std::endl;
    }
    return dbDir;
}

} // namespace construction

