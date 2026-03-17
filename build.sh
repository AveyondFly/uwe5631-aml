#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Configurable variables
ARCH="${ARCH:-arm64}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-rocknix-linux-gnu-}"
KERNEL_SRC="${KERNEL_SRC:?KERNEL_SRC must be set to the kernel build directory}"
CHIP_CONFIG="${CHIP_CONFIG:-CONFIG_RK_WIFI_DEVICE_UWE5621=y}"
WIFI_CFG_PATH="${WIFI_CFG_PATH:-/lib/firmware/unisoc}"

echo "============================================"
echo " UWE5621 Driver Build"
echo "============================================"
echo " ARCH:          ${ARCH}"
echo " CROSS_COMPILE: ${CROSS_COMPILE}"
echo " KERNEL_SRC:    ${KERNEL_SRC}"
echo " CHIP_CONFIG:   ${CHIP_CONFIG}"
echo " WIFI_CFG_PATH: ${WIFI_CFG_PATH}"
echo "============================================"

MAKE_OPTS=(
    ARCH="${ARCH}"
    CROSS_COMPILE="${CROSS_COMPILE}"
    -C "${KERNEL_SRC}"
)

# Step 1: Build BSP (bus abstraction layer)
echo ""
echo ">>> [1/3] Building BSP module (uwe5621_bsp_sdio.ko)..."
make "${MAKE_OPTS[@]}" \
    M="${SCRIPT_DIR}/BSP" \
    "${CHIP_CONFIG}" \
    modules

echo "    BSP build OK."

# Step 2: Build WIFI module (depends on BSP symbols)
echo ""
echo ">>> [2/3] Building WIFI module (sprdwl_ng.ko)..."
make "${MAKE_OPTS[@]}" \
    M="${SCRIPT_DIR}/WIFI" \
    UNISOC_WIFI_CUS_CONFIG="${WIFI_CFG_PATH}" \
    modules

echo "    WIFI build OK."

# Step 3: Build BT module (depends on BSP symbols)
echo ""
echo ">>> [3/3] Building BT module (sprdbt_tty.ko)..."
make "${MAKE_OPTS[@]}" \
    M="${SCRIPT_DIR}/BT/tty-sdio" \
    KERNEL_SRC="${KERNEL_SRC}" \
    CURFOLDER="${SCRIPT_DIR}/BSP" \
    modules

echo "    BT build OK."

echo ""
echo "============================================"
echo " Build complete. Modules:"
echo "============================================"
find "${SCRIPT_DIR}" -name '*.ko' -exec ls -lh {} \;
echo "============================================"
