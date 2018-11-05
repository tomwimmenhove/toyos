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


struct __attribute__((packed)) descr_ptr
{
		uint16_t size;
		uint64_t* base;
};

struct __attribute__((packed)) idt_entry {
		uint16_t offset_1; // offset bits 0..15
		uint16_t selector; // a code segment selector in GDT or LDT
		uint8_t ist;       // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
		uint8_t type_attr; // type and attributes
		uint16_t offset_2; // offset bits 16..31
		uint32_t offset_3; // offset bits 32..63
		uint32_t zero;     // reserved
};/*
		idt_entry()
		{ }

		idt_entry(uint64_t offset, uint16_t selector, uint8_t ist, uint8_t type_attr)
			: offset_1(offset & 0xffff),
			  selector(selector),
			  ist(ist),
			  type_attr(type_attr),
			  offset_2((offset >> 16) & 0xffff),
			  offset_3((offset >> 32) & 0xffffffff),
			  zero(0)
		{ }
};*/

idt_entry idt_entries[256];

struct __attribute__((packed)) gdt_entry
{
	uint64_t l;
};

struct desc_null : public gdt_entry
{
	desc_null()
		: gdt_entry{0}
	{ }
};

struct desc_code_seg : public gdt_entry
{
	desc_code_seg(bool conforming, int privilege, bool present, bool long_mode, bool large_operand)
			: gdt_entry({
					((uint64_t) conforming ? 1llu << (32 + 10) : 0) |
					((uint64_t) (privilege & 3) << (32 + 13)) |
					((uint64_t) present ? 1llu << (32 + 15) : 0) |
					((uint64_t) long_mode ? 1llu << (32 + 21) : 0) |
					((uint64_t) large_operand ? 1llu << (32 + 22) : 0) |
					((uint64_t) 3llu << (32 + 11)) })
			{
				(void) conforming;
				(void) privilege;
				(void) present;
				(void) long_mode;
				(void) large_operand;
			}
};

struct desc_data_seg : public gdt_entry
{
		desc_data_seg(uint32_t base, bool writable, int privilege, bool present)
			: gdt_entry({
					((uint64_t) (base & 0xffff) << 16) |
					((uint64_t) ((base >> 16) & 0xff) << 32) |
					((uint64_t) ((base >> 24) & 0xff) << 56) |
					((uint64_t) writable ? 1llu << (32 + 9) : 0) |
					((uint64_t) (privilege & 3) << (32 + 13)) |
					((uint64_t) present ? 1llu << (32 + 15) : 0) |
					((uint64_t) 1llu << (32 + 12))

					})
			{ }
};

struct __attribute__((packed)) desc_ptr
{
	desc_ptr(gdt_entry* base, uint16_t size)
		: size(size), base(base)
	{ }

	inline void lgdt()
	{
		asm volatile(   "lgdt %0"
				: : "m" (*this));
	}

	inline void lidt()
	{
		asm volatile(   "lidt %0"
				: : "m" (*this));
	}

	uint16_t size;
	gdt_entry* base;
};

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

static inline void lidt(struct descr_ptr* p)
{
	asm volatile(   "lidt %0"
			:
			: "m" (*p));
}

/* The descriptor table */
desc_null kernel_null_descriptor;
desc_code_seg kernel_code_segment_descriptor { false, 0, true, true, false };
desc_data_seg kernel_data_segment_descriptor { 0, true, 0, true };
desc_code_seg interrupt_code_segment_descriptor { false, 0, true, true, false };

gdt_entry gdt_entries[] = {
	kernel_null_descriptor,				// 0x00
	kernel_code_segment_descriptor,		// 0x08
	kernel_data_segment_descriptor,		// 0x10
	interrupt_code_segment_descriptor,	// 0x18
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


	desc_ptr dp { gdt_entries, sizeof(gdt_entries) };
	dp.lgdt();

	asm volatile(	"sub $16, %rsp\n"				// Make space
					"movq $0x8, 8(%rsp)\n"			// Set code segment
					"movabsq $jmp_lbl, %rax\n"		// Set return address (label)...
					"mov %rax, (%rsp)\n"			// ... at stack pointer
					"lretq\n"						// Jump
					"jmp_lbl:");

#if 1
	/* Fill IDT entries */
	for (int i = 0; i < 256; i++)
	{
		uint64_t offset =  ((uint64_t) &irq_0) + i * 0x10;

#if 0
		idt_entries[i] = idt_entry(offset, 0x18, 0, 0x8e);
#else
		auto& e = idt_entries[i];

		e.offset_1 = offset & 0xffff;
		e.offset_2 = (offset >> 16) & 0xffff;
		e.offset_3 = (offset >> 32) & 0xffffffff;

		e.type_attr = 0x8e;

		e.ist = 0;
		e.selector = 0x18;
		e.zero = 0;
#endif
	}

	dbg << "sizeof(idt_entries): " << sizeof(idt_entries) << '\n';

	struct descr_ptr idtp =
	{
			sizeof(idt_entries) - 1,
			(uint64_t*) &idt_entries
	};


//		asm volatile(   "lgdt %0"
//				:
//				: "m" (*(&idtp)));

	lidt(&idtp);

//	*(volatile uint8_t*) 0 = 0;
//	asm("int $42");
//	asm("sti");

	test();

	dbg << "ints work\n";

	for(;;) asm volatile ("hlt");
#endif
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

