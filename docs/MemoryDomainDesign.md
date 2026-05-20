# Memory and Argument System Architecture

## Overview

Akkon has been refactored to support two isolated runtime memory domains and a registration-based argument parsing system.

## Memory Domains

### RUNTIME_META
- **Purpose**: Program state (DB registry, configuration, metadata)
- **Lifecycle**: Persistent for the duration of the program
- **Size**: 10 MB
- **Allocation Strategy**: Bump allocator (arena-based) with O(1) reset

### RUNTIME_REQUEST
- **Purpose**: Temporary request payloads and transient data (for future network request handling)
- **Lifecycle**: Can be reset after each request batch
- **Size**: 5 MB
- **Allocation Strategy**: Arena-based with reset capability

## Component Overview

### `src/memory/Arena.h` / `.cpp`
Simple bump allocator for efficient memory management within a domain:
- `allocate(size, align)`: O(1) allocation with alignment support
- `reset()`: O(1) reset without deallocation (assumes arena is recreated or data is abandoned)
- Tracking: allocation count and used bytes for diagnostics

### `src/memory/MemoryManager.h` / `.cpp`
Central registry managing arena allocation per runtime domain:
- `getDomain(RuntimeDomain)`: Returns the Arena for a given domain
- `resetDomain(RuntimeDomain)`: Resets the arena for a domain
- `printStats()`: Diagnostics on allocation per domain

### `src/shell/ArgumentRegistry.h` / `.cpp`
Registration-based command-line argument parsing:
- **ArgumentMetadata**: Struct defining an argument's properties (name, takes_arg bool, format, help, domain)
- **ArgumentRegistry**: Singleton managing all registered args
- **ParseResult**: Structured result with parsed args, unregistered list, warnings, and error details

Parsing modes:
- **Non-Debug**: Unknown arguments cause an error and exit code 1
- **Debug**: Unknown arguments generate warnings and are skipped; parsing continues

## Usage

### Basic
```bash
./Akkon -c 3 --no-import
```

### With Debug Mode (shows memory stats and arg warnings)
```bash
./Akkon -c 2 --debug
```

### Help
```bash
./Akkon --help
```

## Integration

1. **Shell** creates a MemoryManager on startup
2. **Shell** registers all known arguments via ArgumentRegistry
3. **Shell::run()** calls ArgumentRegistry::parse() with the debug flag
4. DB operations are confined to the RUNTIME_META domain
5. Network requests (future) would use RUNTIME_REQUEST domain with arena resets

## Future Enhancements

- Allocations tracked per DB or request for diagnostics
- Parametric domain sizes (config file)
- Custom allocators for special constraints
- Arena compaction/reorganization for long-running scenarios

