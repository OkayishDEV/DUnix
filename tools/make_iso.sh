#!/usr/bin/env bash
set -e

# DUnix Bootable ISO Image Creator
# Generates a bootable El Torito / isohybrid CD/DVD/USB ISO image with ISOLINUX and DUnix kernel.

OUTPUT_ISO="${1:-build/dunix.iso}"
BUILD_DIR="$(dirname "$OUTPUT_ISO")"
KERNEL_BIN="build/dunix.bin"
ISOLINUX_CFG="tools/isolinux.cfg"
SYSLINUX_DIR="/usr/share/syslinux"

echo "=== Building DUnix Bootable ISO Image ($OUTPUT_ISO) ==="

if [ ! -f "$KERNEL_BIN" ]; then
    echo "Error: $KERNEL_BIN not found. Please run 'make all' first."
    exit 1
fi

# Locate xorriso
XORRISO=""
if command -v xorriso >/dev/null 2>&1; then
    XORRISO="$(command -v xorriso)"
elif [ -x "$HOME/.local/bin/xorriso" ]; then
    XORRISO="$HOME/.local/bin/xorriso"
else
    XORRISO=$(find /nix/store -name "xorriso" -type f -perm -111 2>/dev/null | head -n 1 || true)
fi

if [ -z "$XORRISO" ] || [ ! -x "$XORRISO" ]; then
    echo "Error: xorriso not found."
    exit 1
fi

# Verify Syslinux / ISOLINUX files
for f in "$SYSLINUX_DIR/isolinux.bin" "$SYSLINUX_DIR/isohdpfx.bin" "$SYSLINUX_DIR/mboot.c32" \
         "$SYSLINUX_DIR/ldlinux.c32" "$SYSLINUX_DIR/libcom32.c32" "$SYSLINUX_DIR/libutil.c32" \
         "$SYSLINUX_DIR/menu.c32"; do
    if [ ! -f "$f" ]; then
        echo "Error: ISOLINUX component '$f' not found."
        exit 1
    fi
done

mkdir -p "$BUILD_DIR"
ISO_STAGE="$BUILD_DIR/iso_stage.tmp"
rm -rf "$ISO_STAGE"
mkdir -p "$ISO_STAGE/boot" "$ISO_STAGE/isolinux"

# Step 1: Stage kernel and bootloader files
echo "[1/3] Staging kernel and ISOLINUX components..."
cp "$KERNEL_BIN" "$ISO_STAGE/boot/dunix.bin"
cp "$ISOLINUX_CFG" "$ISO_STAGE/isolinux/isolinux.cfg"
cp "$SYSLINUX_DIR/isolinux.bin" "$ISO_STAGE/isolinux/"
cp "$SYSLINUX_DIR/ldlinux.c32" "$ISO_STAGE/isolinux/"
cp "$SYSLINUX_DIR/libcom32.c32" "$ISO_STAGE/isolinux/"
cp "$SYSLINUX_DIR/libutil.c32" "$ISO_STAGE/isolinux/"
cp "$SYSLINUX_DIR/mboot.c32" "$ISO_STAGE/isolinux/"
cp "$SYSLINUX_DIR/menu.c32" "$ISO_STAGE/isolinux/"

# Step 2: Build isohybrid El Torito ISO
echo "[2/3] Generating hybrid ISO image via xorriso..."
"$XORRISO" -as mkisofs \
  -o "$OUTPUT_ISO" \
  -b isolinux/isolinux.bin \
  -c isolinux/boot.cat \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  -isohybrid-mbr "$SYSLINUX_DIR/isohdpfx.bin" \
  -R -J -joliet-long \
  -V "DUNIX_LIVE_ISO" \
  "$ISO_STAGE" >/dev/null 2>&1

# Create symlink build/dunix-installer.iso -> build/dunix.iso
ln -sf "$(basename "$OUTPUT_ISO")" "$BUILD_DIR/dunix-installer.iso"

# Step 3: Cleanup temporary stage
echo "[3/3] Finalizing image..."
rm -rf "$ISO_STAGE"

SIZE_KB=$(du -k "$OUTPUT_ISO" | cut -f1)
SIZE_MB=$(echo "scale=1; $SIZE_KB / 1024" | bc 2>/dev/null || echo "$((SIZE_KB / 1024))")

echo "Done! Successfully created bootable ISO: $OUTPUT_ISO (${SIZE_MB} MB)"
echo ""
echo "Booting Options:"
echo "  1. Test in QEMU as virtual CD-ROM:"
echo "     qemu-system-x86_64 -cdrom $OUTPUT_ISO -serial stdio -nic model=e1000"
echo "  2. Test in QEMU as raw USB / Hard Drive (isohybrid):"
echo "     qemu-system-x86_64 -drive file=$OUTPUT_ISO,format=raw -serial stdio -nic model=e1000"
echo "  3. Burn to physical CD/DVD or flash to physical USB:"
echo "     sudo dd if=$OUTPUT_ISO of=/dev/sdX bs=4M status=progress conv=fsync"
