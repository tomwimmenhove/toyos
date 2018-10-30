CFLAGS = -O2 -m32 -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs -lgcc
LDFLAGS = -m elf_i386 -T linker.ld 
CC = gcc
AS = as
LD = ld

all: multiboot.elf


boot.o: boot.S
	$(AS) -32 -c boot.S -o boot.o

jump.o: jump.S jump.h
	$(AS) -32 -c jump.S -o jump.o

main.o: main.c ../common/multiboot2.h cpuid.h sysregs.h gdt.h elf.h jump.h ../common/debug_out.h
	$(CC) $(CFLAGS) -c main.c

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

multiboot.elf: debug_out.o boot.o jump.o main.o
	$(LD) $(LDFLAGS) debug_out.o main.o boot.o jump.o -o multiboot.elf

clean:
	rm -rf *.o multiboot.elf

