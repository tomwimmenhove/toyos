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
};

idt_entry idt[256];

/* Figure 4-13 in AMD64 Architecture Programmer’s Manual, Volume 2: System Programming */
static inline uint64_t mk_desc(uint32_t limit, uint32_t base, int type, int s, int privilege, int present, int long_mode, int size_db, int granularity)
{
		uint32_t ldw;
		uint32_t udw;

		ldw = (limit & 0xffff) | ((base & 0xffff) << 16);
		udw = ((base >> 16) & 0xff) | (type << 8) | (s << 12) | (privilege << 13) | (present << 15) | (limit & 0xf0000) | (long_mode << 21) | (size_db << 22) | (granularity << 23) | (base & 0xff000000);

		return (uint64_t) udw << 32 | ldw;
}

static inline void lgdt(struct descr_ptr* p)
{
		asm volatile(   "lgdt %0"
						:
						: "m" (*p));
}

static inline void lidt(struct descr_ptr* p)
{
		asm volatile(   "lidt %0"
						:
						: "m" (*p));
}

extern "C" void interrupt_handler()
{
}

extern "C" void irq_0();
extern "C" void do_test();

int kmain()
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

	uint64_t gdt[] =
	{   
			/* 0x00: Null descriptor */
			0,

			/* 0x08: Code Segment Descriptor */
			mk_desc(    0,//0xfffff,    /* Segment limit */
							0,      /* Base address */
							8,      /* Type: Execute-Only Code-Segment */
							1,      /* S-field: User */
							0,      /* Privilege: 0 */
							1,      /* Present */

							1,      /* Long mode */
							0,//1,      /* Default operand size: ?? */

							0),//1),     /* Granularity: Scale limit by 4096 */

			/* 0x10: Data Segment Descriptor */
			mk_desc(    0xfffff,    /* Segment limit */
							0,      /* Base address */
							2,      /* Type: Read/Write Data-Segment */
							1,      /* S-field: User */
							0,      /* Privilege: 0 */
							1,      /* Present */
							0,      /* Unused */
							1,      /* Default operand size: 32 bit */
							1),     /* Granularity: Scale limit by 4096 */

			/* -------------------------------- */

			/* 0x18: Interrupt Code Segment Descriptor */
			mk_desc(    0xfffff,    /* Segment limit */
							0,      /* Base address */
							0xe,    /* Type 64-bit interrupt gate */
							0,      /* S-field: System */
							0,      /* Privilege: */
							1,      /* Present */

							1,      /* Long mode */
//							1,      /* Default operand size: 64 bit */
							01,      /* Default operand size: 64 bit */

							1),     /* Granularity: Scale limit by 4096 */

			/* 0x20: Interrupt Data Segment Descriptor */
			mk_desc(    0xfffff,    /* Segment limit */
							0,      /* Base address */
							2,      /* Type: Read/Write Data-Segment */
							0,      /* S-field: System */
							0,      /* Privilege: */
							1,      /* Present */
							0,      /* Unused */
							1,      /* Default operand size: 32 bit */
							1),     /* Granularity: Scale limit by 4096 */
	};

	struct descr_ptr gdtp =
	{
			sizeof(gdt) - 1,
			gdt
	};

	lgdt(&gdtp);

	asm volatile(	"sub $16, %rsp\n"				// Make space
					"movq $0x8, 8(%rsp)\n"			// Set code segment
					"movabsq $jmp_lbl, %rax\n"		// Set return address (label)...
					"mov %rax, (%rsp)\n"			// ... at stack pointer
					"lretq\n"						// Jump
					"jmp_lbl:");

//	dbg << "before\n";
//	do_test();
//	dbg << "after\n";


	/* Fill IDT entries */
	for (int i = 0; i < 256; i++)
	{
		uint64_t offset =  ((uint64_t) &irq_0) + i * 0x10;

		auto& e = idt[i];

		e.offset_1 = offset & 0xffff;
		e.offset_2 = (offset >> 16) & 0xffff;
		e.offset_3 = (offset >> 32) & 0xffffffff;

		e.type_attr = 0x80;

		e.ist = 0;
		e.selector = 0x8;
		e.zero = 0;

	}

	dbg << "sizeof(idt): " << sizeof(idt) << '\n';

	struct descr_ptr idtp =
	{
			sizeof(idt) - 1,
			(uint64_t*) &idt
	};

	for (int i = 0; i < 256; i++)
	{   
			uint64_t offset =  ((uint64_t) &irq_0) + i * 0x10;

			auto rax = offset;//((uint64_t) &irq_0);

			auto p = (uint16_t*) &idt[i];

			//mov %ax, idt
			p[0] = rax;

			//movw $0x20, idt+2 // replace 0x20 with your code section selector
			p[1] = 0x8;

			//movw $0x8e00, idt+4
			p[2] = 0x8e00;

			//shr $16, %rax
			rax >>= 16;

			//mov %ax, idt+6
			p[3] = rax;

			//shr $16, %rax
			rax >>= 16;

			//mov %rax, idt+8
			p[4] = rax;
	}

//	for (int i = 0; i < 256; i++)
	for (int i = 0; i < 50; i++)
	{
		uint64_t offset =  ((uint64_t) &irq_0) + i * 0x10;
		auto& e = idt[i];
		dbg << "irq " << " at " << offset << " == " << (e.offset_1 | ((uint64_t) e.offset_2 << 16) | ((uint64_t) e.offset_3 << 32)) << '\n';
	}



//	irq_0();


	lidt(&idtp);

//	asm("sti");
	asm("int $42");

	dbg << "ints work\n";

	for(;;)asm("hlt");


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
}

