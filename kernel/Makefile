CFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs -lgcc
LDFLAGS = -T linker.ld
CC = g++
AS = as
LD = ld

all: kernel.elf

kernel.o: kernel.cpp ../common/debug_out.h
	$(CC) $(CFLAGS) -c kernel.cpp

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

kernel.elf: debug_out.o kernel.o
	$(LD) $(LDFLAGS) debug_out.o kernel.o -o kernel.elf

clean:
	rm -rf *.o kernel.elf

