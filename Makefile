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
           kernel/proc/syscall_asm.S

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
         kernel/proc/process.c \
         kernel/proc/scheduler.c \
         kernel/proc/gdt.c \
         kernel/proc/tss.c \
         kernel/proc/syscall.c \
         kernel/fs/vfs.c \
         kernel/fs/fd.c \
         kernel/fs/stdio.c \
         kernel/fs/fs.c \
         kernel/fs/ramfs/ramfs.c \
         kernel/fs/devfs/devfs.c

# 目标文件
ASM_OBJS = $(ASM_SRCS:.S=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# 目标
.PHONY: all clean run debug

all: myos.iso

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

myos.iso: kernel.bin grub.cfg
	@mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $@ iso 2>/dev/null

# 汇编文件编译
kernel/boot.o: kernel/boot.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/proc/switch.o: kernel/proc/switch.S
	$(AS) $(ASFLAGS) -c $< -o $@

# C 文件编译
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: myos.iso
	./scripts/run.sh

debug: myos.iso
	./scripts/debug.sh

clean:
	rm -rf $(OBJS) kernel.bin myos.iso iso/
	find kernel -name "*.o" -delete
