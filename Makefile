# MyOS Makefile
# 64-bit kernel for x86_64

# 使用系统 gcc (带 -m64)
CC = gcc
AS = gcc
LD = ld

# 编译选项
CFLAGS = -m64 -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -fno-stack-protector -fno-pic -fno-pie \
         -Wall -Wextra -O2 -Iinclude -Ikernel

ASFLAGS = -m64

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000

# 源文件
ASM_SRCS = kernel/boot.S \
           kernel/proc/switch.S \
           kernel/proc/gdt_asm.S \
           kernel/proc/syscall_asm.S \
           kernel/proc/ap_trampoline.S \
           kernel/fs/initramfs_data.S

C_SRCS = kernel/main.c \
         kernel/serial.c \
         kernel/idt.c \
         kernel/pic.c \
         kernel/keyboard.c \
         kernel/lib/string.c \
         kernel/lib/kprintf.c \
         kernel/mm/pmm.c \
         kernel/mm/heap.c \
         kernel/mm/vmm.c \
         kernel/mm/page_fault.c \
         kernel/drivers/pit.c \
         kernel/drivers/pci.c \
         kernel/drivers/virtio.c \
         kernel/drivers/virtio_net.c \
         kernel/drivers/apic.c \
         kernel/drivers/tty.c \
         kernel/proc/process.c \
         kernel/proc/scheduler.c \
         kernel/proc/gdt.c \
         kernel/proc/tss.c \
         kernel/proc/syscall.c \
         kernel/proc/user_space.c \
         kernel/proc/usermode.c \
         kernel/proc/elf.c \
         kernel/proc/clone.c \
         kernel/proc/futex.c \
         kernel/proc/smp.c \
         kernel/proc/dynlink.c \
         kernel/proc/wait_queue.c \
         kernel/proc/syscall_fs.c \
         kernel/proc/syscall_misc.c \
         kernel/lib/spinlock.c \
         kernel/fs/vfs.c \
         kernel/fs/fd.c \
         kernel/fs/stdio.c \
         kernel/fs/fs.c \
         kernel/fs/initramfs.c \
         kernel/fs/poll.c \
         kernel/fs/select.c \
         kernel/fs/ramfs/ramfs.c \
         kernel/fs/devfs/devfs.c \
         kernel/ipc/pipe.c \
         kernel/ipc/signal.c \
         kernel/ipc/shm.c \
         kernel/net/netdev.c \
         kernel/net/ethernet.c \
         kernel/net/arp.c \
         kernel/net/ip.c \
         kernel/net/icmp.c \
         kernel/net/udp.c \
         kernel/net/tcp.c \
         kernel/net/socket.c \
         kernel/net/dhcp.c \
         kernel/net/net.c \
         kernel/drivers/usb/usb.c \
         kernel/drivers/usb/xhci.c \
         kernel/drivers/usb/hid.c \
         kernel/drivers/ide.c \
         kernel/drivers/ahci.c \
         kernel/drivers/acpi.c \
         kernel/fs/ext2/ext2.c \
         kernel/fs/pivot_root.c

# 目标文件
ASM_OBJS = $(ASM_SRCS:.S=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# 目标
.PHONY: all clean run debug userspace initramfs disk run-disk run-disk-virtio debug-disk \
        hybrid usb run-ahci run-acpi test-physical

all: initramfs.cpio myos.iso

# Build userspace programs first
userspace:
	$(MAKE) -C userspace

# Create initramfs archive
initramfs.cpio: userspace
	./scripts/mkinitramfs.sh

# Alias for backwards compatibility
initramfs: initramfs.cpio

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

myos.iso: kernel.bin grub.cfg
	@mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $@ iso 2>/dev/null

# Build everything including userspace
full: userspace initramfs all

# 汇编文件编译
kernel/boot.o: kernel/boot.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/proc/switch.o: kernel/proc/switch.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/proc/gdt_asm.o: kernel/proc/gdt_asm.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/proc/syscall_asm.o: kernel/proc/syscall_asm.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/proc/ap_trampoline.o: kernel/proc/ap_trampoline.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/fs/initramfs_data.o: kernel/fs/initramfs_data.S initramfs.cpio
	$(AS) $(ASFLAGS) -c $< -o $@

# C 文件编译
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: myos.iso
	./scripts/run.sh

debug: myos.iso
	./scripts/debug.sh

# Disk image creation
disk: kernel.bin
	@if [ -f initramfs.cpio ]; then \
		./scripts/mkdisk.sh; \
	else \
		echo "Warning: initramfs.cpio not found, creating without it"; \
		./scripts/mkdisk.sh; \
	fi

# Run from disk image (IDE)
run-disk: disk
	qemu-system-x86_64 \
		-drive file=myos.img,format=raw,if=ide \
		-nographic \
		-no-reboot

# Run from disk image (virtio - better performance)
run-disk-virtio: disk
	qemu-system-x86_64 \
		-drive file=myos.img,format=raw,if=virtio \
		-nographic \
		-no-reboot

# Debug with disk image
debug-disk: disk
	qemu-system-x86_64 \
		-drive file=myos.img,format=raw,if=ide \
		-nographic \
		-no-reboot \
		-s -S

# Create hybrid ISO (bootable from USB and CD)
hybrid: myos.iso
	./scripts/make_hybrid_iso.sh

# Create bootable USB (requires DEVICE=)
usb: myos.iso
	@if [ -z "$(DEVICE)" ]; then \
		echo "Usage: make usb DEVICE=/dev/sdX"; \
		echo ""; \
		echo "Available devices:"; \
		lsblk -d -o NAME,SIZE,MODEL 2>/dev/null | grep -v loop || true; \
		exit 1; \
	fi
	sudo ./scripts/make_usb.sh $(DEVICE)

# Run with AHCI disk controller (for testing AHCI driver)
# Uses Q35 machine type which has native AHCI support
run-ahci: myos.iso
	@if [ ! -f ahci_test.img ]; then \
		qemu-img create -f raw ahci_test.img 128M; \
	fi
	qemu-system-x86_64 \
		-M q35 \
		-m 128M \
		-drive file=ahci_test.img,format=raw,if=virtio \
		-cdrom myos.iso \
		-boot d \
		-serial mon:stdio \
		-display none \
		-no-reboot

# Test poweroff/reboot via ACPI
run-acpi: myos.iso
	qemu-system-x86_64 \
		-cdrom myos.iso \
		-nographic \
		-no-reboot \
		-no-shutdown

# Physical hardware test guide
test-physical:
	./scripts/test_physical.sh

clean:
	rm -rf $(OBJS) kernel.bin myos.iso iso/ initramfs.cpio myos.img
	find kernel -name "*.o" -delete
	$(MAKE) -C userspace clean 2>/dev/null || true
	$(MAKE) -C libc clean 2>/dev/null || true
