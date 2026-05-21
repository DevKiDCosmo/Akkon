#!/bin/bash
# Run a specific test by name
if [ $# -eq 0 ]; then
    echo "Usage: run_test.sh <test_name>"
    echo "Example: run_test.sh test_akkon_basic"
    exit 1
fi

cd "$(dirname "${BASH_SOURCE[0]}")/../cmake-build-debug"
ctest -R "$1" --output-on-failure "$@"

