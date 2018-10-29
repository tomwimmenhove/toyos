CFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs -lgcc -mcmodel=kernel
LDFLAGS = -T linker.ld -z max-page-size=4096
CC = gcc
CPP = g++
AS = as
LD = ld

all: kernel.elf

kernel.o: kernel.cpp ../common/debug_out.h
	$(CPP) $(CFLAGS) -c kernel.cpp

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

kernel.elf: debug_out.o kernel.o
	$(LD) $(LDFLAGS) debug_out.o kernel.o -o kernel.elf

clean:
	rm -rf *.o kernel.elf

