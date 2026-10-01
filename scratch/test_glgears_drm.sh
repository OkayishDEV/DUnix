#!/bin/bash
set -e
rm -f scratch/serial.sock scratch/monitor.sock scratch/gl_drm_screen.ppm

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

# Run glgears in direct DRM mode
(
    echo "glgears --drm &"
    sleep 4.0
) | socat - UNIX-CONNECT:scratch/serial.sock

sleep 1

# Take screenshot
echo "screendump scratch/gl_drm_screen.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1

# Poweroff
echo "poweroff" | socat - UNIX-CONNECT:scratch/serial.sock || true
sleep 1

kill -9 $QEMU_PID 2>/dev/null || true
echo "SUCCESS"
