#!/bin/bash
#
# make_hybrid_iso.sh - Convert MyOS ISO to hybrid (USB/CD bootable)
#
# This script makes the ISO bootable from both CD-ROM and USB drives

set -e

ISO="myos.iso"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${YELLOW}============================================${NC}"
echo -e "${YELLOW}  MyOS Hybrid ISO Creator${NC}"
echo -e "${YELLOW}============================================${NC}"
echo ""

# Check if ISO exists
if [ ! -f "$ISO" ]; then
    echo -e "${RED}Error: $ISO not found${NC}"
    echo "Run 'make' first to build the ISO"
    exit 1
fi

# Check for isohybrid tool
if ! command -v isohybrid &> /dev/null; then
    echo -e "${YELLOW}Installing syslinux-utils for isohybrid...${NC}"
    if command -v apt-get &> /dev/null; then
        sudo apt-get install -y syslinux-utils
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y syslinux
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm syslinux
    else
        echo -e "${RED}Error: Could not install syslinux-utils${NC}"
        echo "Please install it manually and run this script again"
        exit 1
    fi
fi

# Make a backup
cp "$ISO" "${ISO}.bak"
echo "Created backup: ${ISO}.bak"

# Convert to hybrid ISO
echo ""
echo "Converting to hybrid ISO..."
isohybrid "$ISO"

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Done! Hybrid ISO created successfully${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "$ISO can now boot from both:"
echo "  - CD/DVD (burned with any burning software)"
echo "  - USB drive (written with dd or similar)"
echo ""
echo "To write to USB:"
echo "  sudo dd if=$ISO of=/dev/sdX bs=4M status=progress"
echo "  (Replace /dev/sdX with your USB device)"
