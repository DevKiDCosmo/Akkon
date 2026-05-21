#!/bin/bash
# Run only unit tests
cd "$(dirname "${BASH_SOURCE[0]}")/../cmake-build-debug"
ctest -L "^(?!integration|benchmark).*" --output-on-failure "$@"

