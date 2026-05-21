#!/bin/bash
# Run only benchmark tests
cd "$(dirname "${BASH_SOURCE[0]}")/../cmake-build-debug"
ctest -L benchmark --output-on-failure "$@"

