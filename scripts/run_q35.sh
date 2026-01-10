#!/bin/bash
#
# run_q35.sh - Run MyOS in QEMU with Q35 machine (AHCI support)
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO="$PROJECT_DIR/myos.iso"
DISK="$PROJECT_DIR/ahci_test.img"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found. Run 'make' first."
    exit 1
fi

# Create test disk if needed
if [ ! -f "$DISK" ]; then
    qemu-img create -f raw "$DISK" 128M
fi

# Run QEMU with Q35 machine type (has native AHCI)
# Q35 exposes ICH9 AHCI controller at 00:1f.2
qemu-system-x86_64 \
    -M q35 \
    -m 128M \
    -cdrom "$ISO" \
    -drive file="$DISK",format=raw,if=virtio \
    -boot d \
    -serial mon:stdio \
    -display none \
    -no-reboot \
    -no-shutdown \
    "$@"
