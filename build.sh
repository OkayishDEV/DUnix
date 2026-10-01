#!/usr/bin/env bash
set -euo pipefail

echo "========================================"
echo " Building DUnix (x86_64)"
echo "========================================"

mkdir -p build
make clean
make all

echo "========================================"
echo " Build successful: build/dunix.elf"
echo "========================================"
