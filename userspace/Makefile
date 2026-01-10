# Userspace Makefile - Build user programs for MyOS

CC = gcc
LD = ld

LIBC_DIR = ../libc
LIBC = $(LIBC_DIR)/libc.a
CRT0 = $(LIBC_DIR)/crt0.o

CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -Wall -Wextra -O2 -I$(LIBC_DIR)/include

LDFLAGS = -nostdlib -static -z max-page-size=0x1000

# Output directory
BIN_DIR = ../initramfs/bin

# Programs to build
PROGRAMS = init sh echo cat ls pwd mkdir hello

# Targets
.PHONY: all clean libc dirs

all: dirs libc $(PROGRAMS)

# Create output directory
dirs:
	@mkdir -p $(BIN_DIR)

libc:
	$(MAKE) -C $(LIBC_DIR)

init: init/init.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c init/init.c -o init/init.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/init $(CRT0) init/init.o $(LIBC)

sh: shell/sh.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c shell/sh.c -o shell/sh.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/sh $(CRT0) shell/sh.o $(LIBC)

echo: coreutils/echo.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c coreutils/echo.c -o coreutils/echo.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/echo $(CRT0) coreutils/echo.o $(LIBC)

cat: coreutils/cat.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c coreutils/cat.c -o coreutils/cat.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/cat $(CRT0) coreutils/cat.o $(LIBC)

ls: coreutils/ls.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c coreutils/ls.c -o coreutils/ls.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/ls $(CRT0) coreutils/ls.o $(LIBC)

pwd: coreutils/pwd.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c coreutils/pwd.c -o coreutils/pwd.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/pwd $(CRT0) coreutils/pwd.o $(LIBC)

mkdir: coreutils/mkdir.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c coreutils/mkdir.c -o coreutils/mkdir.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/mkdir $(CRT0) coreutils/mkdir.o $(LIBC)

hello: coreutils/hello.c $(LIBC) $(CRT0)
	$(CC) $(CFLAGS) -c coreutils/hello.c -o coreutils/hello.o
	$(LD) $(LDFLAGS) -o $(BIN_DIR)/hello $(CRT0) coreutils/hello.o $(LIBC)

clean:
	rm -f init/*.o shell/*.o coreutils/*.o
	rm -f $(BIN_DIR)/*
	$(MAKE) -C $(LIBC_DIR) clean
