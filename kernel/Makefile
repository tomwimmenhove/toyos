DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

CFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -mcmodel=kernel -mno-red-zone -ggdb $(DEPFLAGS)
CPPFLAGS = -O2 -I ../common -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -fno-exceptions -fno-rtti -std=c++17 -mcmodel=kernel -mno-red-zone -ggdb $(DEPFLAGS)
LDFLAGS = crti.o $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o) \
	  $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o) crtn.o \
	  -T linker.ld -z max-page-size=4096

CC = x86_64-elf-gcc
CPP = x86_64-elf-g++
AS = x86_64-elf-as
LD = x86_64-elf-ld


all: kernel.elf

IOBJS = crti.o crtn.o
OBJS = debug.o   kernel.o memory.o   malloc.o   frame_alloc.o gdt.o   idt.o   interrupts.o   pic.o   console.o   syscall.o interrupt_jump.o new.o
SRCS = debug.cpp kernel.o memory.cpp malloc.cpp frame_alloc.o gdt.cpp idt.cpp interrupts.cpp pic.cpp console.cpp syscall.S interrupt_jump.S new.cpp

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
