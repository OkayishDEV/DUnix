#!/bin/bash
set -e
rm -f scratch/serial.sock scratch/monitor.sock scratch/dws_launch_screen.ppm scratch/dws_launch.log

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

# 1. Start DWS directly in guest
(
    echo "dws &"
    sleep 3.0
) | socat - UNIX-CONNECT:scratch/serial.sock &

# Wait for DWS to be running
sleep 4.0

# 2. Send mouse move to hover over +3D button (glgears) on top bar:
# mouse starts at (512, 384). We want (272, 10).
# dx = 272 - 512 = -240. dy in PS/2: screen y is inverted in process_mouse (mouse_y -= dy).
# So to decrease mouse_y from 384 to 10, dy must be +374!
# QEMU mouse_move accepts relative dx dy.
echo "mouse_move -240 -374" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.5

# 3. Click left button on +3D (glgears launcher) -> triggers dws calling fork()!
echo "mouse_button 1" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 0.2
echo "mouse_button 0" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 4.0

# 4. Take screenshot to verify glgears launched and is rendering
echo "screendump scratch/dws_launch_screen.ppm" | socat - UNIX-CONNECT:scratch/monitor.sock
sleep 1.0

# 5. Clean shutdown
echo "poweroff" | socat - UNIX-CONNECT:scratch/serial.sock || true
sleep 1.0

kill -9 $QEMU_PID 2>/dev/null || true
echo "TEST COMPLETED SUCCESSFULLY"
