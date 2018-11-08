CFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -mcmodel=kernel -ggdb
CPPFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -fno-exceptions -fno-rtti -std=c++17 -mcmodel=kernel -ggdb
LDFLAGS = crti.o $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o) \
	  $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o) crtn.o \
	  -T linker.ld -z max-page-size=4096

CC = x86_64-elf-gcc
CPP = x86_64-elf-g++
AS = x86_64-elf-as
LD = x86_64-elf-ld

all: kernel.elf

IOBJS = crti.o crtn.o
OBJS = debug.o kernel.o memory.o malloc.o frame_alloc.o interrupt_jump.o gdt.o idt.o interrupts.o pic.o console.o syscall.o

%.o: %.S
	$(AS) -c $< -o $@

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c $<

kernel.elf: $(OBJS) $(IOBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.elf

clean:
	rm -rf *.o kernel.elf

