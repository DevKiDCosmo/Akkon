# Deprecated Tests

These tests are from an earlier phase of development and are kept for reference only.

## Files

- `test_ws_simple.py` — Simple WebSocket connection test (deprecated, use C++ tests instead)
- `test_ws_insert.py` — WebSocket insert operation test (deprecated, use C++ tests instead)

## Migration

Use the new C++ test suite in `test_akkon_basic.cpp` and `test_akkon_instance.cpp` with the `TestFramework` class, which provides:
- Process lifecycle management
- Real-time logging
- Status tracking
- PID/Tracker ID management
- CTest integration

These can be run via:
```bash
ctest -R test_akkon
ctest --output-on-failure
```

