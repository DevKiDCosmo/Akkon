# TestFramework runtime() - Quick Reference

## One-Liners

### Run with args only
```cpp
auto exit_code = framework.runtime({"-c", "1"});
```

### Run with environment variables
```cpp
auto exit_code = framework.runtime({"-c", "1"}, {{"DEBUG", "1"}});
```

### Multiple tests
```cpp
assert(framework.runtime({"-c", "1"}) == 0);
assert(framework.runtime({"-c", "2"}) == 0);
assert(framework.runtime({"-p", "9000"}) == 0);
```

## Return Values

| Value | Meaning |
|-------|---------|
| 0 | Success |
| 1-255 | Instance-specific exit code |
| -1 | Critical error / timeout |

## Check Result

```cpp
auto code = framework.runtime(args);
if (code == 0) {
    std::cout << "Success!" << std::endl;
} else if (code < 0) {
    std::cerr << "Critical error!" << std::endl;
} else {
    std::cerr << "Instance failed with code: " << code << std::endl;
}
```

## Features

| Feature | Auto | Manual | Notes |
|---------|------|--------|-------|
| Instance creation | ✅ | - | `createInstance` not needed |
| Environment setup | ✅ | Optional | Pass map for custom vars |
| Container detection | ✅ | - | Automatic |
| Timeout (5 min) | ✅ | - | Built-in protection |
| Cleanup | ✅ | - | Automatic |
| Logging to stderr | ✅ | - | Always enabled |

## Environment Variables

**Auto-set by runtime():**
- `AKKON_IN_CONTAINER`: "1" if containerized
- `AKKON_CONTAINER_ENGINE`: docker/podman/lxc/containerd
- `AKKON_BINARY`: Path to Akkon binary
- `AKKON_TEST_MODE`: "1"
- `AKKON_CWD`: Current working directory

**User can add via second parameter:**
```cpp
std::map<std::string, std::string> env = {
    {"MY_VAR", "value"},
    {"DEBUG", "1"}
};
framework.runtime(args, env);
```

## Common Patterns

### Simple Test
```cpp
int main() {
    TestFramework fw;
    return fw.runtime({"-c", "1"});
}
```

### With Assertion
```cpp
TestFramework fw;
assert(fw.runtime({"-c", "1"}) == 0);
```

### Multiple Sequential Runs
```cpp
TestFramework fw;
for (int i = 1; i <= 5; i++) {
    assert(fw.runtime({"-c", std::to_string(i)}) == 0);
}
```

### Stress Test
```cpp
TestFramework fw;
std::vector<int> loads = {1, 2, 5, 10};
for (auto load : loads) {
    auto code = fw.runtime({"-c", std::to_string(load)});
    std::cout << "Load " << load << ": " << code << std::endl;
}
```

### Container-Aware (Automatic)
```cpp
// No container detection code needed!
// Just call runtime() - it handles everything
auto code = framework.runtime({"-c", "1"});
// stderr will show: "Running in containerized environment"
// or: "Running in native environment"
```

## Error Handling

```cpp
auto code = framework.runtime(args);

// Check for critical error
if (code == -1) {
    std::cerr << "Framework error: check stderr output" << std::endl;
}

// Check for instance failure
if (code != 0) {
    std::cerr << "Instance exited with: " << code << std::endl;
}

// Strict assertion
assert(code >= 0);  // No critical error
assert(code == 0);  // Success
```

## Before → After Comparison

| Task | Before | After |
|------|--------|-------|
| Create instance | `createInstance()` | `runtime()` |
| Wait for completion | `waitForInstance()` | (in `runtime()`) |
| Get exit code | `getExitCode()` | Direct return |
| Stop instance | `stopInstance()` | (in `runtime()`) |
| Setup environment | Manual | `env` parameter |
| Container detection | Manual | (automatic) |

## Debugging

### See logs
- Check `test_logs/` directory for full instance logs
- Instance ID in logs matches what you see in output

### Enable verbose output
```cpp
std::map<std::string, std::string> env = {
    {"AKKON_DEBUG", "1"},
    {"AKKON_LOG_LEVEL", "DEBUG"}
};
auto code = framework.runtime(args, env);
// Check stderr and test_logs/ for details
```

### Check container detection
```cpp
// stderr will show:
// [TestFramework::runtime] Running in containerized environment
// [TestFramework::runtime] Container engine: docker
//
// OR
//
// [TestFramework::runtime] Running in native environment
```

## Timeout Behavior

- **Default:** 5 minutes (300,000 ms)
- **If timeout:** Instance killed, returns -1
- **No override:** Use traditional methods if different timeout needed

## Migration Quick Start

### Step 1: Find this pattern
```cpp
auto id = framework.createInstance(args);
framework.waitForInstance(id, timeout);
auto code = framework.getExitCode(id);
framework.stopInstance(id);
```

### Step 2: Replace with
```cpp
auto code = framework.runtime(args);
```

### Step 3: Update assertions
```cpp
// Before: assert(exit_code == 0)
// After:
assert(framework.runtime(args) == 0);
```

## Compatibility

✅ Works with all existing TestFramework methods
✅ 100% backward compatible
✅ No breaking changes
✅ Mix old and new patterns freely

## File Locations

- **Implementation:** `src/testsuite/TestFramework.{h,cpp}`
- **Full Docs:** `docs/runtime_method.md`
- **Migration Guide:** `docs/MIGRATION_GUIDE.md`
- **Examples:** `tests/example_runtime_usage.cpp`, `tests/test_runtime_examples.cpp`

---

**Quick Start:** `auto code = framework.runtime({"-c", "1"});`

