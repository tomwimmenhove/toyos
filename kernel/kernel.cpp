#include <cstdint>
#include <cstddef>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/config.h"
}

#include "new.h"
#include "mb.h"
#include "memory.h"

extern void* _data_end;
extern void* _code_start;

void die()
{
	asm volatile("movl $0, %eax");
	asm volatile("out %eax, $0xf4");
	asm volatile("cli");
	for (;;)
	{
		asm volatile("hlt");
	}
}

extern "C" void __cxa_pure_virtual()
{
	putstring("Virtual method called\n");
	die();
}

unsigned char answer = 65;

void print_stack_use()
{
	putstring("stack usuage: ");
	put_hex_long(KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0));
	put_char('\n');
}

void parsekbi(kernel_boot_info* kbi)
{
	memory mem(kbi);

	uint64_t phys = 0xb8000;
	uint64_t virt = 0xffffffff40000000 - 0x1000;

	mem.map_page(virt, phys);

	volatile unsigned char* pp = (unsigned char*) virt;
	for (int i = 0; i < 4096; i++)
	{
		pp[i] = 65;
	}

	for(;;)asm("hlt");


	die();
}

int kmain(kernel_boot_info* kbi)
{
	putstring("Kernel info structure at ");
	put_hex_long((uint64_t) kbi);
	put_char('\n');

	if (kbi->magic != KBI_MAGIC)
	{
		putstring("Bad magic number: ");
		put_hex_int(kbi->magic); put_char('\n');
		die();
	}

	parsekbi(kbi);

	for (;;)
		asm("hlt");
}

extern "C" void _start(kernel_boot_info* kbi)
{
	kmain(kbi);
}

