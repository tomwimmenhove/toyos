#include <stdint.h>
#include <stddef.h>

#include "config.h"
#include "linker.h"
#include "new.h"
#include "mb.h"
#include "memory.h"
#include "malloc.h"
#include "frame_alloc.h"
#include "descriptors.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "pic.h"
#include "console.h"
#include "syscall.h"

extern "C" void __cxa_pure_virtual()
{
	panic("Virtual method called");
}

void print_stack_use()
{
	con << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void __attribute__ ((noinline)) test()
{
		asm("int $42");
}

void intr_syscall(uint64_t, interrupt_state* state)
{
	switch(state->rdi)
	{
		case 0:
			con.write_string((const char*) state->rsi);
			break;
		case 1:
			con.putc(state->rsi);
			break;
		case 2:
			asm volatile(
					"sti\n"
					"hlt");
			break;
	}
}

void interrupt_timer(uint64_t, interrupt_state*)
{
//	con << ".";
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);
	con << "key: " << (long long int) ch << "\n";
}

void user_space()
{
	for (int i = 0; i < 10; i++)
		ucon << i << ' ';

	ucon << '\n';

	for (;;)
	{
		for (int i = 0; i < 18; i++)
			syscall(2); // halt
		ucon << '.';
	}
}

volatile uint8_t stack[256];

struct newTest
{
	char s[65536];
	int a;
};

extern "C" void *memset(void *s, int c, size_t n)
{
	uint8_t* p = (uint8_t*) s;

	while (n--)
		*p++ = c;

	return s;
}


void kmain()
{
	mallocator::test();

#if 0
	// XXX: Just a test!
	uint8_t* ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 126 * 1024 * 1024; i += 0x1000)
	{ 
		mem.map_page((uint64_t) ppp, mem.frame_alloc->page());
		ppp += 0x1000;
	} 
	uint64_t* pppp = (uint64_t*) 0xffffa00000000000;
	for (uint64_t i = 0; i < 126 * 1024 * 1024 / sizeof(uint64_t); i++)
		pppp[i] = 0x41414141;
#endif

	interrupts::regist(pic_sys.to_intr(0), interrupt_timer);
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
	interrupts::regist(42, intr_syscall);

	volatile newTest* nt = new newTest();

	nt->a = 42;

	con << "test: " << nt->a << '\n';

	

//	test();

	/* Load TSS0 */
	asm volatile("mov $0x20, %%ax\n"
	"ltr %%ax"
	::: "%ax");

	tss0.rsp0 = ((uint64_t) stack) + sizeof(stack);

	pic_sys.sti();

//	for(;;);
	/* Never return from this */
	asm volatile(
			"mov $0x3b, %%ax\n"
			"mov %%ax, %%ds\n"
			"mov %%ax, %%es\n"
			"mov %%ax, %%fs\n"
			"mov %%ax, %%gs\n"

			"mov %%rsp, %%rax\n"

			"pushq $0x3b\n"	// data seg
			"pushq %%rax\n"	// stack pointer
//			"pushq $2\n" 	// XXX: Check what flags we need?
			"pushfq\n" 		// Push flags
			"pushq $0x33\n"	// code seg

			"pushq %%rbx\n"	// pointer

			"iretq\n"
			:
			: "b" (&user_space)
			:
			);


	for(;;)
	{
		asm volatile ("hlt\nhlt\nhlt\nhlt");
	}

	die();
}

extern "C"
{
	void _init();
	void _fini();

	void _start(kernel_boot_info* kbi);
}

void _start(kernel_boot_info* kbi)
{
	_init();

	if (kbi->magic != KBI_MAGIC)
		panic("Bad magic number!");

	con.init(kbi);
	con << "\n";

	gdt_init();
	idt_init();

	memory::init(kbi);

	/* Map vide text buffer into memory */
	uint64_t video_base = 0xffffffff40000000 - 0x1000;
	memory::map_page(video_base, 0xb8000);
	con.base = (uint8_t*) video_base;

	con << "Video ram remapped\n";

	/* Unmap unused memmory */
	memory::unmap_unused();


	mallocator::init(0xffffa00000000000, 1024 * 1024 * 1024);

	kmain();
	_fini();

	panic("kmain() returned!?");
}

