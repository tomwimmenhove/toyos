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
#include "desctiptors.h"

extern void* _data_end;
extern void* _code_start;

extern "C" void __cxa_pure_virtual()
{
	panic("Virtual method called");
}

unsigned char answer = 65;

void print_stack_use()
{
	dbg << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

idt_entry idt[256];

struct __attribute__((packed)) interrupt_state
{
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;

	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;

	uint64_t rsi;
	uint64_t rdi;

	uint64_t err_code;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state)
{
		dbg << "Interrupt " << irq_num << " err_code: " << state->err_code << " at rip=" << state->rip << '\n';
}

void __attribute__ ((noinline)) test()
{
		asm("int $42");
}

extern "C" void irq_0();
extern "C" void do_test();


struct __attribute__((packed)) gdt_ptr
{
	        uint16_t size;
			        uint64_t* base;
};

void kmain()
{
	mallocator malor(0xffffa00000000000);

	malor.test();


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

	desc_null kernel_null_descriptor;
	desc_code_seg kernel_code_segment_descriptor { false, 0, true, true, false };
	desc_data_seg kernel_data_segment_descriptor { 0, true, 0, true };
	desc_code_seg interrupt_code_segment_descriptor { false, 0, true, true, false };

	gdt_entry descs[] = {
		kernel_null_descriptor,				// 0x00
		kernel_code_segment_descriptor,		// 0x08
		kernel_data_segment_descriptor,		// 0x10
		interrupt_code_segment_descriptor,	// 0x18
	};

	desc_ptr gdt_ptr { descs, sizeof(descs) };
	gdt_ptr.lgdt();

	/* Load the code and data segment */
	asm volatile(	"sub $16, %%rsp\n"				// Make space
					"movq $0x8, 8(%%rsp)\n"			// Set code segment
					"movabsq $jmp_lbl, %%rax\n"		// Set return address (label)...
					"mov %%rax, (%%rsp)\n"			// ... at stack pointer
					"lretq\n"						// Jump
					"jmp_lbl:\n"
					
					"mov $0x10, %%ecx\n"			// Load the data segment
					"mov %%ecx, %%ds\n"				// Stick it in all data-seg regs
					"mov %%ecx, %%es\n"
					"mov %%ecx, %%fs\n"
					"mov %%ecx, %%gs\n"
					"mov %%ecx, %%ss\n"
					: : : "%ecx");

	/* Fill IDT entries */
	for (int i = 0; i < 256; i++)
		idt[i] = idt_entry(((uint64_t) &irq_0) + i * 0x10, 0x18, 0, 0x8e);

	desc_ptr idt_ptr { idt, sizeof(idt) };
	idt_ptr.lidt();

	test();

	dbg << "ints work\n";

	for(;;) asm volatile ("hlt");

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
	if (kbi->magic != KBI_MAGIC)
		panic("Bad magic number!");

	memory::init(kbi);

	_init();
	kmain();
	_fini();

	panic("kmain() returned!?");
}

