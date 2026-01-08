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
         kernel/lib/spinlock.c \
         kernel/fs/vfs.c \
         kernel/fs/fd.c \
         kernel/fs/stdio.c \
         kernel/fs/fs.c \
         kernel/fs/initramfs.c \
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
         kernel/drivers/usb/hid.c

# 目标文件
ASM_OBJS = $(ASM_SRCS:.S=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# 目标
.PHONY: all clean run debug userspace initramfs

all: myos.iso

# Build userspace programs first
userspace:
	$(MAKE) -C userspace

# Create initramfs archive
initramfs: userspace
	./scripts/mkinitramfs.sh

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

kernel/fs/initramfs_data.o: kernel/fs/initramfs_data.S
	$(AS) $(ASFLAGS) -c $< -o $@

# C 文件编译
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: myos.iso
	./scripts/run.sh

debug: myos.iso
	./scripts/debug.sh

clean:
	rm -rf $(OBJS) kernel.bin myos.iso iso/ initramfs.cpio
	find kernel -name "*.o" -delete
	$(MAKE) -C userspace clean 2>/dev/null || true
	$(MAKE) -C libc clean 2>/dev/null || true
