cmake_minimum_required(VERSION 3.20)
project(Akkon)

set(CMAKE_CXX_STANDARD 26)

add_executable(Akkon
	src/main.cpp
	src/memory/ArenaAllocator.cpp
	src/memory/MemoryManager.cpp
	src/crypto/Hash.cpp
	src/index/BloomFilter.cpp
	src/index/QueryEngine.cpp
	src/network/Server.cpp
	src/network/protocols/HttpHandler.cpp
	src/network/protocols/WebSocketHandler.cpp
	src/network/protocols/AkkonProtocolHandler.cpp
	src/monitor/SystemMonitor.cpp
	src/monitor/HealthMonitor.cpp
	src/security/LifecycleManager.cpp
	src/db/DataStorage.cpp
	src/shell/ArgumentParser.cpp
	src/shell/ArgumentRegistry.cpp
	src/shell/Shell.cpp
	src/construction/Utils.cpp
	src/construction/MasterDB.cpp
	src/testsuite/TestFramework.cpp
	sqlite/sqlite3.c
)
target_include_directories(Akkon PRIVATE sqlite/ src/)
target_compile_options(Akkon PRIVATE -pthread)

enable_testing()
add_executable(test_arena_allocator tests/test_arena_allocator.cpp src/memory/ArenaAllocator.cpp)
target_include_directories(test_arena_allocator PRIVATE src/)
add_test(NAME test_arena_allocator COMMAND test_arena_allocator)

add_executable(test_bloom_filter tests/test_bloom_filter.cpp src/index/BloomFilter.cpp src/index/QueryEngine.cpp src/crypto/Hash.cpp)
target_include_directories(test_bloom_filter PRIVATE src/)
add_test(NAME test_bloom_filter COMMAND test_bloom_filter)

add_executable(test_network tests/test_network.cpp src/network/Server.cpp src/monitor/SystemMonitor.cpp src/security/LifecycleManager.cpp)
target_include_directories(test_network PRIVATE src/)
add_test(NAME test_network COMMAND test_network)

add_executable(test_availability tests/test_availability.cpp src/network/Server.cpp src/monitor/SystemMonitor.cpp src/security/LifecycleManager.cpp)
target_include_directories(test_availability PRIVATE src/)
add_test(NAME test_availability COMMAND test_availability)

add_executable(test_concurrency tests/test_concurrency.cpp src/index/QueryEngine.cpp src/index/BloomFilter.cpp src/crypto/Hash.cpp)
target_include_directories(test_concurrency PRIVATE src/)
add_test(NAME test_concurrency COMMAND test_concurrency)

add_executable(test_memory_isolation tests/test_memory_isolation.cpp src/memory/MemoryManager.cpp src/memory/ArenaAllocator.cpp)
target_include_directories(test_memory_isolation PRIVATE src/)
add_test(NAME test_memory_isolation COMMAND test_memory_isolation)

add_executable(test_hashing tests/test_hashing.cpp src/crypto/Hash.cpp)
target_include_directories(test_hashing PRIVATE src/)
add_test(NAME test_hashing COMMAND test_hashing)

add_executable(benchmark_1m
	tests/benchmark_1m.cpp
	src/index/BloomFilter.cpp
	src/index/QueryEngine.cpp
	src/crypto/Hash.cpp
)
target_include_directories(benchmark_1m PRIVATE src/)
add_test(NAME benchmark_1m COMMAND benchmark_1m)
set_tests_properties(benchmark_1m PROPERTIES LABELS benchmark)

# TestFramework-based integration tests
add_executable(test_akkon_basic
	tests/test_akkon_basic.cpp
	src/testsuite/TestFramework.cpp
)
target_include_directories(test_akkon_basic PRIVATE src/)
add_test(NAME test_akkon_basic COMMAND test_akkon_basic)
set_tests_properties(test_akkon_basic PROPERTIES LABELS integration)
set_tests_properties(test_akkon_basic PROPERTIES ENVIRONMENT "AKKON_BINARY=$<TARGET_FILE:Akkon>")

add_executable(test_akkon_instance
	tests/test_akkon_instance.cpp
	src/testsuite/TestFramework.cpp
)
target_include_directories(test_akkon_instance PRIVATE src/)
add_test(NAME test_akkon_instance COMMAND test_akkon_instance)
set_tests_properties(test_akkon_instance PROPERTIES LABELS integration)
set_tests_properties(test_akkon_instance PROPERTIES ENVIRONMENT "AKKON_BINARY=$<TARGET_FILE:Akkon>")

add_executable(test_database
	tests/test_database.cpp
	sqlite/sqlite3.c
)
target_include_directories(test_database PRIVATE sqlite/ src/)
target_compile_options(test_database PRIVATE -pthread)
add_test(NAME test_database COMMAND test_database)
set_tests_properties(test_database PROPERTIES LABELS integration)
set_tests_properties(test_database PROPERTIES ENVIRONMENT "AKKON_BINARY=$<TARGET_FILE:Akkon>")


