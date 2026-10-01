#!/bin/bash
set -e
rm -f scratch/serial.sock scratch/monitor.sock scratch/gl_screen.ppm

qemu-system-x86_64 \
    -cdrom build/dunix.iso \
    -serial unix:scratch/serial.sock,server,nowait \
    -monitor unix:scratch/monitor.sock,server,nowait \
    -nic model=e1000 \
    -display none &
QEMU_PID=$!

while [ ! -S scratch/serial.sock ] || [ ! -S scratch/monitor.sock ]; do
    sleep 0.2
done

# Boot wait
sleep 4.5

# 1. Launch DWS and glgears in guest
(
    echo "dsession &"
    sleep 2.5
    echo "glgears &"
    sleep 6.0
) | socat - UNIX-CONNECT:scratch/serial.sock &

# 2. Wait until glgears is actively rendering frames
sleep 5.5

# 3. Take screenshot via QEMU monitor
echo "screendump scratch/gl_screen.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0

# 4. Clean shutdown
echo "poweroff" | socat - UNIX-CONNECT:scratch/serial.sock || true
sleep 1.0

kill -9 $QEMU_PID 2>/dev/null || true
echo "SUCCESS"
