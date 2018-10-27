#!/bin/bash

CFLAGS = -O2 -m32 -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs
LDFLAGS = -m32 -T linker.ld
CC = gcc
AS = as
LD = gcc

all: multiboot.elf


boot.o: boot.S
	$(AS) -32 -c boot.S -o boot.o

main.o: main.c multiboot2.h cpuid.h sysregs.h gdt.h elf.h ../common/debug_out.h
	$(CC) $(CFLAGS) -c main.c

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

multiboot.elf: debug_out.o boot.o main.o
	$(LD) $(LDFLAGS) $(CFLAGS) debug_out.o main.o boot.o -lgcc -o multiboot.elf

clean:
	rm -rf *.o multiboot.elf

