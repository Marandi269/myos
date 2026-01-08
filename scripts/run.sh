#!/bin/bash
#
# run.sh - Run MyOS in QEMU
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO="$PROJECT_DIR/myos.iso"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found. Run 'make' first."
    exit 1
fi

# Run QEMU with serial output to terminal
# -serial mon:stdio allows serial + QEMU monitor on stdio
# -display none disables graphical display (headless mode)
# -netdev user,id=net0 enables user-mode networking
# -device virtio-net-pci,netdev=net0 adds virtio-net device
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -serial mon:stdio \
    -display none \
    -m 128M \
    -no-reboot \
    -no-shutdown \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0 \
    "$@"
