#!/bin/bash
set -e

rm -f scratch/serial.sock scratch/monitor.sock scratch/wbar_step1.ppm scratch/wbar_step2.ppm scratch/wbar_step*.png

echo "=== 1. Starting QEMU ==="
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

sleep 4.5

echo "=== 2. Starting DWS and glgears ==="
(
    echo "dws &"
    sleep 2.5
    echo "glgears &"
    sleep 3.0
) | socat - UNIX-CONNECT:scratch/serial.sock &

sleep 4.5

echo "=== 3. Taking screenshot with 3D window active ==="
echo "screendump scratch/wbar_step1.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0
convert scratch/wbar_step1.ppm scratch/wbar_step1.png

echo "=== 4. Clicking window titlebar close button [x] ==="
# Window titlebar close button is at x=490, y=70.
# Total dx = -20, total dy = -320. Split into 3 steps:
echo "mouse_move -7 -107" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.1
echo "mouse_move -7 -107" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.1
echo "mouse_move -6 -106" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.5
echo "mouse_button 1" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.2
echo "mouse_button 0" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 2.0

echo "=== 5. Taking screenshot after clicking window titlebar close button ==="
echo "screendump scratch/wbar_step2.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0
convert scratch/wbar_step2.ppm scratch/wbar_step2.png

echo "=== 6. Powering off ==="
echo "poweroff" | socat - UNIX-CONNECT:scratch/serial.sock || true
sleep 1.0

echo "=== Test Completed! ==="
