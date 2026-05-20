cmake_minimum_required(VERSION 4.2)
project(Akkon)

set(CMAKE_CXX_STANDARD 26)

add_executable(Akkon
	src/main.cpp
	src/memory/Arena.cpp
	src/memory/MemoryManager.cpp
	src/shell/ArgumentParser.cpp
	src/shell/ArgumentRegistry.cpp
	src/shell/Shell.cpp
	src/construction/Utils.cpp
	src/construction/MasterDB.cpp
	sqlite/sqlite3.c
)
target_include_directories(Akkon PRIVATE sqlite/ src/)
target_compile_options(Akkon PRIVATE -pthread)
