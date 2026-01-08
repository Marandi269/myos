# libc Makefile - Minimal C library for MyOS

CC = gcc
AR = ar
AS = gcc

CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -Wall -Wextra -O2 -Iinclude

ASFLAGS = -m64

# Source files
C_SRCS = src/string.c \
         src/stdlib.c \
         src/stdio.c \
         src/unistd.c \
         src/wait.c \
         src/dirent.c \
         src/signal.c \
         src/mman.c \
         src/pipe.c \
         src/pthread.c

ASM_SRCS = src/crt0.S

# Object files
C_OBJS = $(C_SRCS:.c=.o)
ASM_OBJS = $(ASM_SRCS:.S=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# Output
LIBC = libc.a
CRT0 = crt0.o

.PHONY: all clean

all: $(LIBC) $(CRT0)

$(LIBC): $(C_OBJS)
	$(AR) rcs $@ $^

$(CRT0): src/crt0.S
	$(AS) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIBC) $(CRT0)
	find . -name "*.o" -delete
