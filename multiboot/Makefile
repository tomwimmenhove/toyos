CFLAGS = -O2 -m32 -I ../newlib/c++/4.9.3/ -I ../newlib/ -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs -lgcc
LDFLAGS = -m elf_i386 -T linker.ld 
CC = x86_64-elf-gcc
CPP = x86_64-elf-g++
AS = x86_64-elf-gcc # Handles defines through the cpp pre-processor, as does not.
LD = x86_64-elf-ld

all: multiboot.elf


boot.o: boot.S
	$(AS) -m32 -c boot.S -o boot.o

jump.o: jump.S jump.h
	$(AS) -m32 -c jump.S -o jump.o

main.o: main.c ../common/multiboot2.h cpuid.h sysregs.h gdt.h elf.h jump.h ../common/debug_out.h
	$(CC) $(CFLAGS) -c main.c

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

multiboot.elf: debug_out.o boot.o jump.o main.o
	$(LD) $(LDFLAGS) debug_out.o main.o boot.o jump.o -o multiboot.elf

clean:
	rm -rf *.o multiboot.elf

