#!/usr/bin/env bash
set -e

# DUnix Bootable USB Installer Image Creator
# Generates a bootable MBR/FAT16 USB disk image with Syslinux and DUnix kernel.

OUTPUT_IMG="${1:-build/dunix-installer.img}"
BUILD_DIR="$(dirname "$OUTPUT_IMG")"
KERNEL_BIN="build/dunix.bin"
SYSLINUX_CFG="tools/syslinux.cfg"
SYSLINUX_DIR="/usr/share/syslinux"

echo "=== Building DUnix Bootable USB Installer Image ==="

if [ ! -f "$KERNEL_BIN" ]; then
    echo "Error: $KERNEL_BIN not found. Please run 'make all' first."
    exit 1
fi

for cmd in mkfs.vfat syslinux sfdisk mcopy dd; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: Required tool '$cmd' is not installed."
        exit 1
    fi
done

for f in "$SYSLINUX_DIR/mbr.bin" "$SYSLINUX_DIR/mboot.c32" "$SYSLINUX_DIR/libcom32.c32" "$SYSLINUX_DIR/libutil.c32" "$SYSLINUX_DIR/menu.c32"; do
    if [ ! -f "$f" ]; then
        echo "Error: Syslinux module '$f' not found."
        exit 1
    fi
done

mkdir -p "$BUILD_DIR"
PART_IMG="$BUILD_DIR/part1.tmp"
MBR_GAP="$BUILD_DIR/gap.tmp"

# 1. Create 63 MiB Partition Image (FAT16)
echo "[1/5] Creating 63 MiB FAT16 partition..."
dd if=/dev/zero of="$PART_IMG" bs=1M count=63 status=none
mkfs.vfat -F 16 -n "DUNIX_INST" "$PART_IMG" >/dev/null

# 2. Install Syslinux on partition
echo "[2/5] Installing Syslinux bootloader..."
syslinux -i "$PART_IMG"

# 3. Populate partition with Syslinux modules, config, and DUnix kernel
echo "[3/5] Deploying kernel and bootloader modules..."
mcopy -o -i "$PART_IMG" "$SYSLINUX_DIR/mboot.c32" ::/mboot.c32
mcopy -o -i "$PART_IMG" "$SYSLINUX_DIR/libcom32.c32" ::/libcom32.c32
mcopy -o -i "$PART_IMG" "$SYSLINUX_DIR/libutil.c32" ::/libutil.c32
mcopy -o -i "$PART_IMG" "$SYSLINUX_DIR/menu.c32" ::/menu.c32
mcopy -o -i "$PART_IMG" "$KERNEL_BIN" ::/dunix.bin
mcopy -o -i "$PART_IMG" "$SYSLINUX_CFG" ::/syslinux.cfg

# 4. Assemble 64 MiB MBR Disk Image (1 MiB MBR gap + 63 MiB partition)
echo "[4/5] Assembling 64 MiB MBR disk image ($OUTPUT_IMG)..."
dd if=/dev/zero of="$MBR_GAP" bs=1M count=1 status=none
cat "$MBR_GAP" "$PART_IMG" > "$OUTPUT_IMG"

# Write MBR boot code (first 440 bytes)
dd if="$SYSLINUX_DIR/mbr.bin" of="$OUTPUT_IMG" bs=440 count=1 conv=notrunc status=none

# Partition table: 1 bootable FAT16 partition starting at sector 2048 (1 MiB offset)
printf '2048,,0x06,*\n' | sfdisk "$OUTPUT_IMG" >/dev/null 2>&1

# Update Syslinux partition offset in VBR
syslinux --offset 1048576 -i "$OUTPUT_IMG"

# 5. Cleanup temporary files
rm -f "$PART_IMG" "$MBR_GAP"

SIZE_MB=$(du -m "$OUTPUT_IMG" | cut -f1)
echo "[5/5] Done! Created bootable USB installer image: $OUTPUT_IMG ($SIZE_MB MB)"
echo ""
echo "To write to a physical USB drive:"
echo "  sudo dd if=$OUTPUT_IMG of=/dev/sdX bs=4M status=progress conv=fsync"
echo ""
echo "To test in QEMU:"
echo "  qemu-system-x86_64 -drive file=$OUTPUT_IMG,format=raw -serial stdio -nic model=e1000"
