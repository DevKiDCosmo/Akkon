# Akkon Server Architecture Implementation Plan

This plan outlines the architecture and implementation steps to build the Akkon server based on your requirements. The project is an in-memory focused server that persists data using multiple SQLite databases, designed to be more lightweight than PostgreSQL while providing high-performance access.

## Proposed Architecture Overview

1.  **Memory Management (Arena Allocator)**: Custom memory manager utilizing an Arena allocator with 64-byte aligned block sizes (64n) to maximize cache utilization and performance. All working data will reside in RAM.
2.  **Data Modeling & Identification**: All identification fields (email, password, username, etc.) will be hashed using SHA-256 to ensure a consistent length and improve security/lookup speeds. Secondary/metadata will remain in plain text.
3.  **Probabilistic Query Engine**: To achieve the "is there", "is probably there", and "there is no" states, we will use a **Bloom Filter** (or a combination of Cuckoo filters/Bloom filters). This provides O(1) checks for "definitely no" and "probably yes", falling back to exact checks in the memory arena for "definitely yes".
4.  **Multi-Interface Network Layer**: A unified network dispatcher handling multiple protocols:
    *   Custom `akkon://` protocol
    *   REST (HTTP)
    *   Raw Sockets (TCP/UDP)
    *   WebSocket (WSS)
    *   SSH / FTP equivalents
5.  **Watchdog & Logger Subprocess**: A separate subprocess decoupled from the main memory manager. It will continuously monitor the health of the main process, handle asynchronous logging, and ensure data integrity between RAM and the SQLite databases.

## Proposed Changes

---

### Memory Manager & Arena Allocator
#### [NEW] `src/memory/ArenaAllocator.h`
#### [NEW] `src/memory/ArenaAllocator.cpp`
- Implement a 64-byte aligned arena allocator.
- Pre-allocate large contiguous memory blocks.
- Provide custom placement `new` and `delete` operators for data structures.

---

### Security & Data Modeling
#### [NEW] `src/crypto/Hash.h`
#### [NEW] `src/crypto/Hash.cpp`
- Implement SHA-256 hashing utilities (wrapping OpenSSL or a lightweight SHA-256 implementation).
- Provide functions to hash identifiers (username, email, password).

---

### Probabilistic Query Engine
#### [NEW] `src/index/BloomFilter.h`
#### [NEW] `src/index/BloomFilter.cpp`
#### [NEW] `src/index/QueryEngine.h`
#### [NEW] `src/index/QueryEngine.cpp`
- Implement a Bloom filter to support the three states: "No", "Probably Yes", and exact resolution for "Yes".
- Integrate with the Arena Allocator to check actual presence when the filter returns "Probably Yes".

---

### Network Interfaces
#### [NEW] `src/network/Server.h`
#### [NEW] `src/network/Server.cpp`
#### [NEW] `src/network/protocols/HttpHandler.h`
#### [NEW] `src/network/protocols/WebSocketHandler.h`
#### [NEW] `src/network/protocols/AkkonProtocolHandler.h`
- Create an asynchronous event loop (e.g., using `epoll`/`kqueue` or a library like `Boost.Asio` if allowed, otherwise custom POSIX sockets).
- Implement parsers and responders for the requested interfaces.

---

### Watchdog & Logger Subprocess
#### [NEW] `src/watchdog/Watchdog.h`
#### [NEW] `src/watchdog/Watchdog.cpp`
#### [MODIFY] `src/logger/Logger.h`
#### [MODIFY] `src/logger/Logger.cpp`
- Refactor the existing logger.
- Implement `fork()` (or platform-equivalent) during startup to spawn the Watchdog process.
- Use IPC (pipes or shared memory) for communication between the Main process and the Watchdog.

---

### Main Application Logic
#### [MODIFY] `src/main.cpp`
- Initialize the Arena allocator.
- Fork the Watchdog subprocess.
- Initialize the networking server.
- Update the startup sequence to load data from `MasterDB` into the RAM memory manager.

## Open Questions

> [!WARNING]
> Please review these questions as they will impact the implementation:

1.  **Dependencies**: Are we allowed to use third-party C++ libraries (e.g., OpenSSL for SHA-256, Boost.Asio for networking, or a websocket library)? Or should everything be implemented from scratch using POSIX C APIs and standard C++26?
2.  **Concurrency**: Should the memory manager and query engine be thread-safe to support multi-threaded network workers, or will there be a single main event loop with asynchronous I/O?
3.  **Data Structure in RAM**: Aside from the 64-byte aligned arena, what specific data structures (e.g., Hash Maps, B-Trees) should be used to store the loaded SQLite data in RAM?
4.  **Watchdog Responsibilities**: Should the watchdog be responsible for *flushing* RAM changes back to SQLite, or is the main process doing the writing and the watchdog just monitoring for crashes?

## Verification Plan
### Automated Tests
- Unit tests for the Arena Allocator to ensure strict 64-byte alignment and correct memory usage.
- Unit tests for SHA-256 hashing.
- Tests for the Bloom filter to verify the "Is there", "Probably there", and "No" states.
### Manual Verification
- Compile and run the server.
- Connect using curl (REST) and custom socket clients (`akkon://`) to verify responses.
- Simulate a crash of the main process and verify that the Watchdog detects it and logs appropriately.
