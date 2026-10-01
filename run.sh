#!/usr/bin/env bash
set -euo pipefail

KERNEL="build/dunix.bin"

if [ ! -f "$KERNEL" ]; then
    echo "Kernel binary not found. Building first..."
    ./build.sh
fi

MODE="${1:-default}"

case "$MODE" in
    "curses")
        echo "Launching DUnix in QEMU (curses mode)..."
        qemu-system-x86_64 -kernel "$KERNEL" -display curses -serial stdio
        ;;
    "nographic"|"serial")
        echo "Launching DUnix in QEMU (nographic / serial mode)..."
        qemu-system-x86_64 -kernel "$KERNEL" -nographic -serial mon:stdio
        ;;
    "debug")
        echo "Launching DUnix in QEMU (GDB debug mode on :1234)..."
        qemu-system-x86_64 -kernel "$KERNEL" -s -S -serial stdio
        ;;
    *)
        echo "Launching DUnix in QEMU..."
        qemu-system-x86_64 -kernel "$KERNEL" -serial stdio
        ;;
esac
