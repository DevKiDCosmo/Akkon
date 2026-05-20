#pragma once
#include <string>
#include <filesystem>

namespace construction {

std::string generateUUID();
std::string getCurrentTimestamp();
std::string escapeSqlString(const std::string& input);
std::filesystem::path ensureDbDirectory();

} // namespace construction

