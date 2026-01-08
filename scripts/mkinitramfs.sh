#!/bin/bash
#
# mkinitramfs.sh - Create initramfs CPIO archive
#
# Usage: ./scripts/mkinitramfs.sh [output_file]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
INITRAMFS_DIR="$ROOT_DIR/initramfs"
OUTPUT="${1:-$ROOT_DIR/initramfs.cpio}"

# Create initramfs directory structure if not exists
mkdir -p "$INITRAMFS_DIR/bin"
mkdir -p "$INITRAMFS_DIR/etc"
mkdir -p "$INITRAMFS_DIR/dev"

# Create a motd file
cat > "$INITRAMFS_DIR/etc/motd" << 'EOF'
Welcome to MyOS!
A Unix-like Operating System
EOF

# Generate CPIO archive
echo "Creating initramfs archive..."
cd "$INITRAMFS_DIR"
find . | cpio -o -H newc > "$OUTPUT" 2>/dev/null

echo "Created: $OUTPUT ($(stat -c%s "$OUTPUT") bytes)"
ls -la "$OUTPUT"
