DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

HACKFLAGS = -fno-threadsafe-statics # Until we implement the safe guards
DEBUGFLAGS = -ggdb -DDEBUG
PRIMALFLAGS = -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -msoft-float
INCLUDEDIRS = -I ../../kernel/ -I ../../common -I ../../newlib/c++/4.9.3/ -I ../../newlib/ -I ../../embxx/
CFLAGS = $(HACKFLAGS) $(DEBUGFLAGS) -O2 $(INCLUDEDIRS) -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -ffunction-sections -fdata-sections -Wl,gc-sections $(DEPFLAGS)
CPPFLAGS = $(HACKFLAGS) $(DEBUGFLAGS) -O2 $(INCLUDEDIRS) -Wall -Wextra -Werror -nostdlib -ffreestanding -lgcc -ffunction-sections -fdata-sections -Wl,gc-sections $(DEPFLAGS) -fno-exceptions -fno-rtti -std=c++17 -fconcepts
LDFLAGS = 

CC = x86_64-elf-gcc
CPP = x86_64-elf-g++
AS = x86_64-elf-gcc # Handles defines through the cpp pre-processor, gas does not.
LD = x86_64-elf-ld

all: userbin

OBJS = main.o syscall.o
SRCS = main.cpp

syscall.o: ../../kernel/syscall.S
	$(AS) -c ../../kernel/syscall.S -o syscall.o

%.o: %.S
	$(AS) -c $< -o $@

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c $<
	$(POSTCOMPILE)

userbin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o userbin

clean:
	rm -rf *.o userbin

$(DEPDIR)/%.d: ;
	.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
