# TestFramework runtime() Method

## Overview

The `runtime()` method is an all-in-one solution for running Akkon instances in tests. It consolidates instance creation, execution, container detection, and cleanup into a single method call, significantly reducing boilerplate code in test files.

## Features

### 1. **Container Detection**
- Automatically detects if running inside a containerized environment
- Supports Docker, Podman, LXC, and containerd
- Checks for:
  - `.dockerenv` file
  - Container-specific cgroup entries
  - Container socket availability

### 2. **Container Engine Detection**
- Identifies which containerization software is available
- Returns: `docker`, `podman`, `containerd`, `lxc`, or `none`
- Useful for environment-specific adjustments

### 3. **Environment Variable Management**
- Automatic setup of test environment variables
- Supports custom environment variables passed by caller
- Sets:
  - `AKKON_IN_CONTAINER`: "1" if containerized
  - `AKKON_CONTAINER_ENGINE`: detected engine name
  - `AKKON_BINARY`: path to Akkon binary
  - `AKKON_TEST_MODE`: "1"
  - `AKKON_CWD`: current working directory

### 4. **Automatic Instance Lifecycle**
- Creates instance with provided arguments
- Waits for completion (default 5-minute timeout)
- Returns exit code directly
- Logs status and final output to stderr

### 5. **Automatic Cleanup**
- Stops instance after completion
- No need for manual cleanup calls

## Method Signature

```cpp
std::int32_t runtime(
    const std::vector<std::string>& args, 
    const std::map<std::string, std::string>& env = {}
);
```

### Parameters
- `args`: Command-line arguments for the Akkon instance
- `env`: Optional map of environment variables (merged with auto-detected ones)

### Returns
- Exit code of the instance (0 on success)
- Negative value (-1) on error

## Usage Examples

### Basic Usage
```cpp
TestFramework framework;
std::vector<std::string> args = {"-c", "1"};
std::int32_t exit_code = framework.runtime(args);

if (exit_code == 0) {
    std::cout << "Success!" << std::endl;
} else {
    std::cout << "Failed with code: " << exit_code << std::endl;
}
```

### With Custom Environment Variables
```cpp
std::map<std::string, std::string> env = {
    {"AKKON_DEBUG", "1"},
    {"AKKON_LOG_LEVEL", "DEBUG"},
    {"MY_CUSTOM_VAR", "value"}
};

std::int32_t exit_code = framework.runtime(args, env);
```

### In Unit Tests
```cpp
int main() {
    TestFramework framework;
    
    // Test 1: Basic execution
    std::vector<std::string> args1 = {"-c", "1"};
    assert(framework.runtime(args1) == 0);
    
    // Test 2: With custom settings
    std::vector<std::string> args2 = {"-p", "8080", "-c", "2"};
    std::map<std::string, std::string> env = {
        {"AKKON_DEBUG", "1"}
    };
    assert(framework.runtime(args2, env) == 0);
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
```

## Before vs After

### Before: Verbose Manual Management
```cpp
TestFramework framework;
std::vector<std::string> args = {"-c", "1"};

// Manual instance creation
std::string tracker_id = framework.createInstance(args, "test_logs");
assert(!tracker_id.empty());

// Manual wait and status check
bool completed = framework.waitForInstance(tracker_id, 10000);
assert(completed);

// Manual exit code retrieval
std::int32_t exit_code = framework.getExitCode(tracker_id);
auto logs = framework.getLogs(tracker_id, 10);

// Manual cleanup
framework.stopInstance(tracker_id);
```

### After: Simplified Single Call
```cpp
TestFramework framework;
std::vector<std::string> args = {"-c", "1"};
std::int32_t exit_code = framework.runtime(args);
// That's it!
```

## Container-Aware Testing

The `runtime()` method automatically adapts to containerized environments:

```cpp
// Same code works in container and native environments:
TestFramework framework;
std::vector<std::string> args = {"-c", "1"};
std::int32_t exit_code = framework.runtime(args);

// stderr will show:
// [TestFramework::runtime] Running in containerized environment
// [TestFramework::runtime] Container engine: docker
//
// OR
//
// [TestFramework::runtime] Running in native environment
```

## Timeout Behavior

- Default timeout: 5 minutes (300,000 ms)
- If instance doesn't complete within timeout:
  - Instance is forcefully killed
  - Method returns -1
  - Error message logged to stderr

## Error Handling

The method returns:
- **0 or positive**: Exit code from the instance
- **-1**: Critical error during setup or timeout

Check for errors:
```cpp
std::int32_t exit_code = framework.runtime(args);
if (exit_code < 0) {
    // Critical error occurred
    std::cerr << "Failed to run instance!" << std::endl;
} else if (exit_code != 0) {
    // Instance completed but with non-zero exit
    std::cerr << "Instance exited with code: " << exit_code << std::endl;
}
```

## Implementation Details

### Helper Methods

Three private helper methods support the main `runtime()` function:

1. **`isContainerized()`**: Checks if running in container
   - Looks for .dockerenv, cgroup entries

2. **`detectContainerEngine()`**: Identifies container software
   - Checks for Docker/Podman sockets
   - Supports docker, podman, containerd, lxc

3. **`setupEnvironment()`**: Configures environment
   - Merges provided env with auto-detected settings
   - Sets standard test variables
   - Applies to current process for child inheritance

## Benefits

1. **Reduced Code**: 40-60% less boilerplate in test files
2. **Consistency**: All tests use same initialization pattern
3. **Container-Aware**: Automatically detects and adapts to environments
4. **Easier Maintenance**: Single point of configuration for instance lifecycle
5. **Better Logging**: Automatic stderr output for debugging
6. **Timeout Protection**: Built-in timeout prevents hanging tests

## Migration Guide

To migrate existing tests to use `runtime()`:

### Step 1: Identify Pattern
Look for this pattern in test files:
```cpp
createInstance() → wait/check status → getExitCode → stopInstance
```

### Step 2: Replace with runtime()
```cpp
// Before
std::string id = framework.createInstance(args);
framework.waitForInstance(id, timeout);
int code = framework.getExitCode(id);
framework.stopInstance(id);

// After
int code = framework.runtime(args);
```

### Step 3: Update Assertions
```cpp
// Before
assert(exit_code == 0 && "Instance failed");

// After  
assert(framework.runtime(args) == 0);
```

## Compatibility

- **C++ Standard**: C++17 or later (requires std::map, std::filesystem)
- **OS Support**: Windows, Linux, macOS
- **Thread-Safe**: Single-threaded per instance (see m_instances map for multi-instance scenarios)

## Future Enhancements

Potential improvements:
- Custom timeout specification per invocation
- Callback hooks for status monitoring
- Resource usage tracking
- Integration with CI/CD environments
- Log file compression
- Instance pooling/reuse

