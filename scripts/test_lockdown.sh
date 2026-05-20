#!/bin/bash
if [ -z "$1" ]; then
    echo "Usage: ./test_lockdown.sh [SAFE|CLOSE|IMMEDIATE]"
    exit 1
fi
echo "$1" > vuln_server/status.txt
echo "Vulnerability status set to: $1"
