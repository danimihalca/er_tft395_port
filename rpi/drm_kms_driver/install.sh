#!/bin/sh
# Build/install the lt7680-drm module (DKMS) and the er-tft395 overlay on Raspberry Pi OS.
set -e

[ "$(id -u)" = 0 ] || { echo "Run with sudo."; exit 1; }

HERE=$(cd "$(dirname "$0")" && pwd)
VER=0.1
BOOT=/boot/firmware
[ -d "$BOOT/overlays" ] || BOOT=/boot

apt-get update
apt-get install -y dkms device-tree-compiler
# header package name differs between Bookworm and Trixie
apt-get install -y "linux-headers-$(uname -r)" 2>/dev/null || apt-get install -y raspberrypi-kernel-headers

rm -rf "/usr/src/lt7680-drm-$VER"
mkdir -p "/usr/src/lt7680-drm-$VER"
cp -r "$HERE"/dkms/* "/usr/src/lt7680-drm-$VER/"

dkms remove "lt7680-drm/$VER" --all 2>/dev/null || true
dkms add -m lt7680-drm -v "$VER"
dkms build -m lt7680-drm -v "$VER"
dkms install -m lt7680-drm -v "$VER"

dtc -@ -I dts -O dtb -o "$BOOT/overlays/er-tft395.dtbo" "$HERE/dts/er-tft395-overlay.dts"

echo
echo "Done. Add this line to $BOOT/config.txt, power off, wire the display, then reboot:"
echo "    dtoverlay=er-tft395"
