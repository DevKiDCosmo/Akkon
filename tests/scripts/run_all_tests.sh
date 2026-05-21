#!/bin/bash
# Run all tests
cd "$(dirname "${BASH_SOURCE[0]}")/../cmake-build-debug"
ctest --output-on-failure "$@"

