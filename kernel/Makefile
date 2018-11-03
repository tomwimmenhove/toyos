CFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -mcmodel=kernel
CPPFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -fno-exceptions -fno-rtti -std=c++17 -mcmodel=kernel
LDFLAGS = crti.o $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o) \
	  $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o) crtn.o \
	  -T linker.ld -z max-page-size=4096

CC = x86_64-elf-gcc
CPP = x86_64-elf-g++
AS = x86_64-elf-as
LD = x86_64-elf-ld

all: kernel.elf

kernel.o: kernel.cpp ../common/debug_out.h
	$(CPP) $(CPPFLAGS) -c kernel.cpp

crti.o: crti.S
	$(AS) -c crti.S -o crti.o

crtn.o: crtn.S
	$(AS) -c crtn.S -o crtn.o

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

memory.o: memory.cpp memory.h frame_alloc.h
	$(CPP) $(CPPFLAGS) -c memory.cpp

frame_alloc.o: frame_alloc.cpp frame_alloc.h memory.h
	$(CPP) $(CPPFLAGS) -c frame_alloc.cpp

kernel.elf: crti.o crtn.o debug_out.o kernel.o memory.o frame_alloc.o
	$(LD) $(LDFLAGS) debug_out.o kernel.o memory.o frame_alloc.o  -o kernel.elf

clean:
	rm -rf *.o kernel.elf

