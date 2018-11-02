CFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs -lgcc -mcmodel=kernel
CPPFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -nostartfiles -nodefaultlibs -lgcc -fno-exceptions -fno-rtti -std=c++17 -mcmodel=kernel
LDFLAGS = -T linker.ld -z max-page-size=4096
CC = gcc
CPP = g++
AS = as
LD = ld

all: kernel.elf

kernel.o: kernel.cpp ../common/debug_out.h
	$(CPP) $(CPPFLAGS) -c kernel.cpp

debug_out.o: ../common/debug_out.c ../common/debug_out.h
	$(CC) $(CFLAGS) -c ../common/debug_out.c

memory.o: memory.cpp memory.h
	$(CPP) $(CPPFLAGS) -c memory.cpp

kernel.elf: debug_out.o kernel.o memory.o
	$(LD) $(LDFLAGS) debug_out.o kernel.o memory.o -o kernel.elf

clean:
	rm -rf *.o kernel.elf

