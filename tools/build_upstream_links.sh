#!/bin/bash
# ==============================================================================
# build_upstream_links.sh - Fetch and Build Upstream Twibright Links 2.30
# Builds genuine, unmodified Links browser statically with Musl libc for DUnix.
# ==============================================================================
set -e

LINKS_VER="2.30"
TAR_FILE="links-${LINKS_VER}.tar.bz2"
SRC_URL="http://links.twibright.com/download/${TAR_FILE}"

BUILD_DIR="$(pwd)/build/links_src"
OUTPUT_BIN="$(pwd)/user/linux_apps/bin/links"

echo "=== Building Genuine Upstream Links ${LINKS_VER} for DUnix Linuxulator ==="
mkdir -p "${BUILD_DIR}"
mkdir -p "$(dirname "${OUTPUT_BIN}")"

if [ ! -f "${BUILD_DIR}/${TAR_FILE}" ]; then
    echo "[1/4] Downloading Links ${LINKS_VER} source..."
    curl -L -s -o "${BUILD_DIR}/${TAR_FILE}" "${SRC_URL}"
fi

if [ ! -d "${BUILD_DIR}/links-${LINKS_VER}" ]; then
    echo "[2/4] Extracting source..."
    tar -xjf "${BUILD_DIR}/${TAR_FILE}" -C "${BUILD_DIR}"
fi

echo "[3/4] Configuring Links (Static, Text Mode)..."
mkdir -p "${BUILD_DIR}/obj"
cd "${BUILD_DIR}/obj"

# Locate musl-gcc from nix store or host
MUSL_CC=$(which x86_64-unknown-linux-musl-gcc 2>/dev/null || find /nix/store -name "x86_64-unknown-linux-musl-gcc" 2>/dev/null | head -n 1)

if [ -z "${MUSL_CC}" ]; then
    echo "Error: x86_64-unknown-linux-musl-gcc not found. Please install musl static toolchain."
    exit 1
fi

CC="${MUSL_CC}" \
CFLAGS="-O2 -static" \
LDFLAGS="-static" \
../links-${LINKS_VER}/configure \
    --host=x86_64-unknown-linux-musl \
    --without-x \
    --without-fb \
    --without-directfb \
    --without-svgalib \
    --without-ssl \
    --without-zlib \
    --without-brotli \
    --without-zstd \
    --without-bzip2 \
    --without-lzma \
    --without-lzip \
    --without-gpm \
    --without-libevent

echo "[4/4] Compiling Links..."
make -j"$(nproc)"
strip -s links
cp links "${OUTPUT_BIN}"

echo "=== Successfully built genuine upstream Links at: ${OUTPUT_BIN} ==="
file "${OUTPUT_BIN}"
ls -lh "${OUTPUT_BIN}"
