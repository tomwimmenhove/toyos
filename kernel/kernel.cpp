#include <stdint.h>
#include <stddef.h>

extern "C"
{
	#include "../common/config.h"
}

#include "debug.h"
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

extern void* _data_end;
extern void* _code_start;

extern "C" void __cxa_pure_virtual()
{
	panic("Virtual method called");
}

void print_stack_use()
{
	dbg << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void __attribute__ ((noinline)) test()
{
		asm("int $42");
}

void interrupt_42(uint64_t irq_num, interrupt_state* state)
{
//	(void) irq_num; (void) state;
//	for (;;);
	dbg << "Interrupt " << irq_num << " at rip=" << state->iregs.rip << '\n';
}

void interrupt_timer(uint64_t, interrupt_state*)
{
	dbg << '.';
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);
	dbg << "key: " << ch << '\n';
}

void user_space()
{
	asm("int $42");

	for (;;) ;
//	dbg << "Hello world\n";
	
//	for (;;) asm volatile("cli\nhlt");
}

volatile uint8_t stack[256];

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

	uint64_t phys = 0xb8000;
	uint64_t virt = 0xffffffff40000000 - 0x1000;

	memory::map_page(virt, phys);

	volatile unsigned char* pp = (unsigned char*) virt;
	for (int i = 0; i < 4096; i++)
	{
		pp[i] = 65;
	}

//	*(uint8_t*) 42 = 42;

	pic_sys.disable(0x20);
//	pic_sys.enable(0x21);
//	pic_sys.pic1.set_mask(0x00);

	interrupts::regist(pic_sys.to_intr(0), interrupt_timer);
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
	interrupts::regist(42, interrupt_42);

	

//	test();

	/* Load TSS0 */
	asm volatile("mov $0x20, %%ax\n"
	"ltr %%ax"
	::: "%ax");

	tss0.rsp0 = ((uint64_t) stack) + sizeof(stack);

	dbg << "after\n";

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

	gdt_init();
	idt_init();

	memory::init(kbi);
	mallocator::init(0xffffa00000000000, 1024 * 1024 * 1024);

	kmain();
	_fini();

	panic("kmain() returned!?");
}

