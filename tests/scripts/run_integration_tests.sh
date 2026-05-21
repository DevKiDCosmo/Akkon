#!/bin/bash
# Run only integration tests (TestFramework-based tests)
cd "$(dirname "${BASH_SOURCE[0]}")/../cmake-build-debug"
ctest -L integration --output-on-failure "$@"

