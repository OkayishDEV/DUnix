#!/bin/bash
set -e

rm -f scratch/serial.sock scratch/monitor.sock scratch/links_test.log

echo "=== 1. Starting QEMU with DUnix ISO ==="
qemu-system-x86_64 \
    -cdrom build/dunix.iso \
    -serial unix:scratch/serial.sock,server,nowait \
    -monitor unix:scratch/monitor.sock,server,nowait \
    -nic model=e1000 \
    -display none &
QEMU_PID=$!

cleanup() {
    kill -9 $QEMU_PID 2>/dev/null || true
}
trap cleanup EXIT

while [ ! -S scratch/serial.sock ] || [ ! -S scratch/monitor.sock ]; do
    sleep 0.2
done

# Wait for boot
sleep 4.5

echo "=== 2. Testing Links under Linuxulator ==="
(
    echo "linux -v"
    sleep 1.0
    echo "linux -i /compat/linux/bin/links"
    sleep 1.0
    echo "links -version"
    sleep 1.0
    echo "links -help | head -n 15"
    sleep 1.0
    echo "echo '<h1>Genuine Upstream Links 2.30</h1><p>Running native Linux binary via Linuxulator</p>' > /test.html"
    sleep 1.0
    echo "links -dump /test.html"
    sleep 1.0
    echo "ifconfig"
    sleep 1.0
    echo "ping 10.0.2.2"
    sleep 2.0
    echo "poweroff"
) | socat - UNIX-CONNECT:scratch/serial.sock > scratch/links_test.log 2>&1 || true

echo "=== Log Output ==="
cat scratch/links_test.log
echo "=== Test script finished ==="
