#pragma once
#include <string>

namespace db {
    struct DbEntity {
        std::string uuid;       // SHA-256 hex string (64 characters)
        std::string identifyer; // SHA-256 hex string (64 characters)
        std::string pwk;        // SHA-256 hex string (64 characters)
    };
} // namespace db
