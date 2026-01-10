#!/bin/bash
#
# make_usb.sh - Create bootable USB drive for MyOS
#
# Usage: ./scripts/make_usb.sh /dev/sdX
#
# WARNING: This will DESTROY all data on the target device!

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check arguments
if [ $# -ne 1 ]; then
    echo -e "${YELLOW}Usage: $0 <device>${NC}"
    echo "Example: $0 /dev/sdb"
    echo ""
    echo "Available block devices:"
    lsblk -d -o NAME,SIZE,MODEL | grep -v loop
    exit 1
fi

DEVICE=$1
ISO="myos.iso"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
    exit 1
fi

# Check if device exists
if [ ! -b "$DEVICE" ]; then
    echo -e "${RED}Error: $DEVICE is not a block device${NC}"
    exit 1
fi

# Check if ISO exists
if [ ! -f "$ISO" ]; then
    echo -e "${RED}Error: $ISO not found${NC}"
    echo "Run 'make' first to build the ISO"
    exit 1
fi

# Get device info
DEVICE_SIZE=$(lsblk -b -d -o SIZE "$DEVICE" | tail -1)
DEVICE_MODEL=$(lsblk -d -o MODEL "$DEVICE" | tail -1)

echo -e "${YELLOW}============================================${NC}"
echo -e "${YELLOW}  MyOS USB Boot Creator${NC}"
echo -e "${YELLOW}============================================${NC}"
echo ""
echo "Target device: $DEVICE"
echo "Model: $DEVICE_MODEL"
echo "Size: $(numfmt --to=iec-i --suffix=B $DEVICE_SIZE)"
echo ""
echo -e "${RED}WARNING: ALL DATA ON $DEVICE WILL BE DESTROYED!${NC}"
echo ""

# Double confirmation
read -p "Are you sure you want to continue? (Type 'yes' to confirm): " confirm
if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# Make sure device is unmounted
echo ""
echo "Unmounting partitions..."
umount ${DEVICE}* 2>/dev/null || true

# Write ISO to device
echo ""
echo -e "${GREEN}Writing $ISO to $DEVICE...${NC}"
dd if="$ISO" of="$DEVICE" bs=4M status=progress conv=fsync

# Sync to ensure all data is written
sync

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Done! USB boot drive created successfully${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "To boot from this USB drive:"
echo "1. Insert the USB drive into your target computer"
echo "2. Enter BIOS/UEFI setup (usually F2, F12, or Del during boot)"
echo "3. Select the USB drive as the boot device"
echo "4. Boot and enjoy MyOS!"
echo ""
echo "Note: You may need to disable Secure Boot for UEFI systems"
