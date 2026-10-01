#!/bin/bash
set -e

rm -f scratch/serial.sock scratch/monitor.sock scratch/mouse_step1.ppm scratch/mouse_step2.ppm scratch/mouse_step*.png

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
echo "screendump scratch/mouse_step1.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0
convert scratch/mouse_step1.ppm scratch/mouse_step1.png

echo "=== 4. Clicking top-bar close button [x] ==="
# Top bar close button for focused window is at x=748, y=10.
# Mouse starts at (512, 384).
# In test_dws_launcher.sh: dx = 272 - 512 = -240, dy = -374 moved from 384 down to 10.
# So to reach (748, 10): dx = 748 - 512 = +236, dy = -374.
echo "mouse_move 236 -374" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.5
echo "mouse_button 1" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.2
echo "mouse_button 0" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 2.0

echo "=== 5. Taking screenshot after clicking close button ==="
echo "screendump scratch/mouse_step2.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0
convert scratch/mouse_step2.ppm scratch/mouse_step2.png

echo "=== 6. Powering off ==="
echo "poweroff" | socat - UNIX-CONNECT:scratch/serial.sock || true
sleep 1.0

echo "=== Test Completed! ==="
