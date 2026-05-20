#pragma once
#include <cstdint>
namespace db {
    // Must fit perfectly within 64-byte Arena blocks
    struct DbEntity {
        uint64_t uuid;       // 64b - foreign key to other DBs for plain text/encoded info
        uint64_t identifyer; // 64b - Must be a hash of the real identifier (e.g. Email)
        uint64_t pwk;        // 64b - Must be a hash of the real password

        // 24 bytes used. The rest is padding up to 64 bytes if allocated inside Arena

        // Design Philosophy: All PII must be hashed before storage.
        // Example: identifyer = hash_to_uint64("user@email.com")
    };
} // namespace db
