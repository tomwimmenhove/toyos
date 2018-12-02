DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

HACKFLAGS = -fno-threadsafe-statics # Until we implement the safe guards
DEBUGFLAGS = -ggdb -DDEBUG -fstack-protector-all
PRIMALFLAGS = -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -msoft-float
INCLUDEDIRS = -I ../common -I ../newlib/c++/4.9.3/ -I ../newlib/ -I ../embxx/
CFLAGS = $(HACKFLAGS) $(DEBUGFLAGS) -O2 $(INCLUDEDIRS) -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -mcmodel=kernel -mno-red-zone -ffunction-sections -fdata-sections -Wl,gc-sections $(DEPFLAGS)
CPPFLAGS = $(HACKFLAGS) $(DEBUGFLAGS) -O2 $(INCLUDEDIRS) -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -mcmodel=kernel -mno-red-zone -ffunction-sections -fdata-sections -Wl,gc-sections $(DEPFLAGS) -fno-exceptions -fno-rtti -std=c++17 -fconcepts
LDFLAGS = crti.o $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o) \
	  $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o) crtn.o \
	  -T linker.ld -z max-page-size=4096

CC = x86_64-elf-gcc
CPP = x86_64-elf-g++
AS = x86_64-elf-gcc # Handles defines through the cpp pre-processor, gas does not.
LD = x86_64-elf-ld

all: kernel.elf

IOBJS = crti.o crtn.o
OBJS = debug.o   kernel.o mem.o   malloc.o   frame_alloc.o gdt.o   idt.o   interrupts.o   pic.o   console.o   syscall.o interrupt_jump.o new.o   task_helper.o klib.o   start.o   dev.o   kbd.o   ata_pio.o   iso9660.o   elf.o
SRCS = debug.cpp kernel.o mem.cpp malloc.cpp frame_alloc.o gdt.cpp idt.cpp interrupts.cpp pic.cpp console.cpp syscall.S interrupt_jump.S new.cpp task_helper.S klib.cpp start.cpp dev.cpp kbd.cpp ata_pio.cpp iso9660.cpp elf.cpp

start.o: start.cpp
	$(CPP) $(CPPFLAGS) $(PRIMALFLAGS) -c start.cpp
	$(POSTCOMPILE)

%.o: %.S
	$(AS) -c $< -o $@

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c $<
	$(POSTCOMPILE)

kernel.elf: $(OBJS) $(IOBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.elf

clean:
	rm -rf *.o kernel.elf

$(DEPDIR)/%.d: ;
	.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
