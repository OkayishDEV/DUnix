#!/bin/bash
set -e

rm -f scratch/serial.sock scratch/monitor.sock scratch/step1_gl_opened.ppm scratch/step2_gl_closed.ppm scratch/*.png

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

# Allow kernel to boot to shell
sleep 4.5

echo "=== 2. Starting DWS and glgears ==="
(
    echo "dws &"
    sleep 2.5
    echo "glgears --seconds 4 &"
    sleep 3.0
) | socat - UNIX-CONNECT:scratch/serial.sock &

sleep 4.5

echo "=== 3. Capturing Screenshot 1: 3D Window Active ==="
echo "screendump scratch/step1_gl_opened.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0

if [ -f scratch/step1_gl_opened.ppm ]; then
    convert scratch/step1_gl_opened.ppm scratch/step1_gl_opened.png
    echo "Screenshot 1 captured: scratch/step1_gl_opened.png"
fi

echo "=== 4. Waiting for glgears to complete and close window ==="
# glgears was started with --seconds 4, so after 4 seconds it exits and calls dui_destroy_window
sleep 3.5

echo "=== 5. Capturing Screenshot 2: 3D Window Closed & Gone ==="
echo "screendump scratch/step2_gl_closed.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0

if [ -f scratch/step2_gl_closed.ppm ]; then
    convert scratch/step2_gl_closed.ppm scratch/step2_gl_closed.png
    echo "Screenshot 2 captured: scratch/step2_gl_closed.png"
fi

echo "=== 6. Powering off ==="
echo "poweroff" | socat - UNIX-CONNECT:scratch/serial.sock || true
sleep 1.0

echo "=== Verification complete! ==="
