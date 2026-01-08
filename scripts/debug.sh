#!/bin/bash
#
# debug.sh - Run MyOS in QEMU with GDB debugging support
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO="$PROJECT_DIR/myos.iso"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found. Run 'make' first."
    exit 1
fi

echo "Starting QEMU with GDB server on port 1234..."
echo "Connect with: gdb -ex 'target remote :1234' -ex 'symbol-file kernel.bin'"

# Run QEMU with GDB server
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -serial stdio \
    -m 128M \
    -s -S \
    -no-reboot \
    "$@"
