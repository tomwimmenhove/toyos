#include "debug.h"
#include "console.h"

void die()
{
	asm volatile("cli");
#if 0
	asm volatile("movl $0, %eax");
	asm volatile("out %eax, $0xf4");
	asm volatile("cli");
#endif
	for (;;)
	{
		asm volatile("hlt");
	}
}

void panic(const char* msg)
{
	con << "KERNEL PANIC: " << msg << "\nHalted.";;
	die();
}

