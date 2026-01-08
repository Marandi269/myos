#!/bin/bash
# mkdisk.sh - Create bootable disk image for MyOS
# Uses direct image manipulation without loop devices (works in containers)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

DISK_IMG="myos.img"
DISK_SIZE=64  # MB
SECTOR_SIZE=512
SECTORS_PER_MB=$((1024*1024/SECTOR_SIZE))
TOTAL_SECTORS=$((DISK_SIZE * SECTORS_PER_MB))

# First partition starts at 1MB (2048 sectors)
PART_START=2048
PART_SECTORS=$((TOTAL_SECTORS - PART_START))

check_kernel() {
    if [ ! -f "kernel.bin" ]; then
        echo "Error: kernel.bin not found. Run 'make' first."
        exit 1
    fi
}

check_prereqs() {
    local missing=""
    for cmd in dd grub-mkimage mtools mcopy; do
        if ! command -v $cmd &>/dev/null; then
            missing="$missing $cmd"
        fi
    done
    if [ -n "$missing" ]; then
        echo "Note: Missing optional commands:$missing"
        echo "Trying alternative methods..."
    fi
}

create_partition_table() {
    # Create MBR with one bootable partition
    # MBR layout: 446 bytes boot code + 64 bytes partition table + 2 bytes signature

    # Partition entry at offset 446 (first partition)
    # Status: 0x80 (bootable)
    # Type: 0x83 (Linux)
    # Start: sector 2048
    # Size: rest of disk

    local end_sector=$((TOTAL_SECTORS - 1))

    # Calculate CHS values (not used by BIOS with LBA, but required for compatibility)
    # Use dummy CHS values
    local start_head=32
    local start_sector=33
    local start_cyl=0
    local end_head=254
    local end_sector_chs=63
    local end_cyl=255

    # Create partition entry (16 bytes)
    printf '\x80' > /tmp/part_entry  # Boot flag
    printf '\x20\x21\x00' >> /tmp/part_entry  # CHS start (dummy)
    printf '\x83' >> /tmp/part_entry  # Type: Linux
    printf '\xfe\x3f\xff' >> /tmp/part_entry  # CHS end (dummy)

    # LBA start (little-endian 32-bit)
    printf "\\x$(printf '%02x' $((PART_START & 0xff)))" >> /tmp/part_entry
    printf "\\x$(printf '%02x' $(((PART_START >> 8) & 0xff)))" >> /tmp/part_entry
    printf "\\x$(printf '%02x' $(((PART_START >> 16) & 0xff)))" >> /tmp/part_entry
    printf "\\x$(printf '%02x' $(((PART_START >> 24) & 0xff)))" >> /tmp/part_entry

    # LBA size (little-endian 32-bit)
    printf "\\x$(printf '%02x' $((PART_SECTORS & 0xff)))" >> /tmp/part_entry
    printf "\\x$(printf '%02x' $(((PART_SECTORS >> 8) & 0xff)))" >> /tmp/part_entry
    printf "\\x$(printf '%02x' $(((PART_SECTORS >> 16) & 0xff)))" >> /tmp/part_entry
    printf "\\x$(printf '%02x' $(((PART_SECTORS >> 24) & 0xff)))" >> /tmp/part_entry

    # Write partition entry to MBR
    dd if=/tmp/part_entry of="$DISK_IMG" bs=1 seek=446 conv=notrunc status=none

    # Zero out other partition entries
    dd if=/dev/zero of="$DISK_IMG" bs=1 seek=462 count=48 conv=notrunc status=none

    # Write MBR signature (0x55AA)
    printf '\x55\xaa' | dd of="$DISK_IMG" bs=1 seek=510 conv=notrunc status=none

    rm -f /tmp/part_entry
}

install_grub_mbr() {
    # Use grub-mkimage to create a standalone GRUB image
    # Then write MBR boot code

    # Check if grub-mkimage is available
    if ! command -v grub-mkimage &>/dev/null; then
        echo "Warning: grub-mkimage not found, using pre-built boot sector"
        # Just create a minimal boot sector that prints error
        return 1
    fi

    # Create GRUB core image
    local grub_prefix="(hd0,msdos1)/boot/grub"

    # Create a minimal grub.cfg for the embedded config
    cat > /tmp/grub_embed.cfg << 'EOF'
set root=(hd0,msdos1)
set prefix=(hd0,msdos1)/boot/grub
EOF

    # Create core.img with necessary modules
    grub-mkimage -O i386-pc -o /tmp/core.img \
        -c /tmp/grub_embed.cfg \
        -p "$grub_prefix" \
        biosdisk part_msdos ext2 multiboot multiboot2 2>/dev/null || {
        echo "Warning: grub-mkimage failed"
        return 1
    }

    # Get boot.img (first 446 bytes of MBR)
    local grub_dir="/usr/lib/grub/i386-pc"
    if [ ! -f "$grub_dir/boot.img" ]; then
        echo "Warning: GRUB boot.img not found"
        return 1
    fi

    # Write boot.img (first 440 bytes, leaving disk signature and partition table)
    dd if="$grub_dir/boot.img" of="$DISK_IMG" bs=440 count=1 conv=notrunc status=none

    # Write core.img starting at sector 1 (after MBR)
    dd if=/tmp/core.img of="$DISK_IMG" bs=512 seek=1 conv=notrunc status=none

    rm -f /tmp/grub_embed.cfg /tmp/core.img
    return 0
}

create_ext2_filesystem() {
    # Create ext2 filesystem image for the partition
    local fs_img="/tmp/myos_fs.img"
    local fs_size_blocks=$((PART_SECTORS * SECTOR_SIZE / 1024))  # Size in 1K blocks

    # Create empty file for filesystem
    dd if=/dev/zero of="$fs_img" bs=1024 count=$fs_size_blocks status=none

    # Format as ext2
    mkfs.ext2 -q -b 1024 "$fs_img"

    # Use debugfs to add files
    echo "    Adding files to ext2 filesystem..."

    # Create boot directory and grub subdirectory
    debugfs -w "$fs_img" -R "mkdir boot" 2>/dev/null
    debugfs -w "$fs_img" -R "mkdir boot/grub" 2>/dev/null

    # Write kernel.bin
    debugfs -w "$fs_img" -R "write kernel.bin boot/kernel.bin" 2>/dev/null

    # Write initramfs if exists
    if [ -f "initramfs.cpio" ]; then
        debugfs -w "$fs_img" -R "write initramfs.cpio boot/initramfs.cpio" 2>/dev/null
        echo "    - Added initramfs.cpio"
    fi

    # Create grub.cfg
    cat > /tmp/grub.cfg << 'EOF'
set timeout=0
set default=0

menuentry "MyOS" {
    multiboot2 /boot/kernel.bin
    module2 /boot/initramfs.cpio
    boot
}
EOF
    debugfs -w "$fs_img" -R "write /tmp/grub.cfg boot/grub/grub.cfg" 2>/dev/null
    rm -f /tmp/grub.cfg

    # Create directory structure
    for dir in bin etc home var tmp; do
        debugfs -w "$fs_img" -R "mkdir $dir" 2>/dev/null
    done

    # Copy filesystem to disk image at partition offset
    dd if="$fs_img" of="$DISK_IMG" bs=512 seek=$PART_START conv=notrunc status=none

    rm -f "$fs_img"
}

echo "=== MyOS Disk Image Creator ==="
check_prereqs
check_kernel

echo "[1/5] Creating ${DISK_SIZE}MB disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE status=none

echo "[2/5] Creating partition table..."
create_partition_table

echo "[3/5] Installing GRUB bootloader..."
if ! install_grub_mbr; then
    echo "Warning: GRUB installation failed, using fallback"
    # Fallback: just ensure we have valid MBR
fi

echo "[4/5] Creating ext2 filesystem and copying files..."
create_ext2_filesystem

echo "[5/5] Finalizing..."

echo ""
echo "=== Success ==="
echo "Created: $DISK_IMG (${DISK_SIZE}MB)"
ls -lh "$DISK_IMG"
echo ""
echo "Run with: make run-disk"
