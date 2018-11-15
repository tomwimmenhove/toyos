#include <stdint.h>

#include "console.h"
#include "debug.h"
#include "idt.h"
#include "gdt.h"
#include "config.h"
#include "mem.h"
#include "malloc.h"
#include "cpuid.h"
#include "sysregs.h"
#include "fpu.h"

extern "C"
{
	void _init();
	void _fini();

	void _start(kernel_boot_info* kbi);
}

void kmain();

#define silent_death() for (;;)	asm volatile("cli\nhlt\n")

void cpu_init()
{
	if (!has_cpuid())
		silent_death();
	uint64_t a, b, c, d;
	cpuid(1, 0, a, b, c, d);

	if (!(d & 1))			// CPU has no FPU
		silent_death();
	if (!(d & (1 << 23)))	// CPU has no MMX extensions
		silent_death();
	if (!(d & (1 << 25)))	// CPU has no SSE extensions
		silent_death();
	if (!(d & (1 << 26)))	// CPU has no SSE2 extensions
		silent_death();
	if (!(d & (1 << 24)))	// CPU does not support FXSAVE and FXRSTOR Instructions
		silent_death();

//	if (!(c & (1 << 26)))	// CPU does not support XSAVE, XRESTOR, XSETBV, XGETBV
//		silent_death();
//	if (!(c & (1 << 27)))	// CPU does not support OSXSAVE
//		silent_death();

	uint64_t cr0 = cr0_get();
	cr0 &= ~(1 << 2);	// Clear EM bit
	cr0 |= (1 << 5);	// Set NE bit (Native Exceptions)
	cr0 &= ~(1 << 3);	// Clear TS bit (no NM exception in kernel mode)
	cr0 |= (1 << 1);	// Set MP bit (FWAIT exempt from the TS bit)
	cr0_set(cr0);

	fpu::fninit();
	uint16_t cw = fpu::fstcw();
	cw &= ~1;			// Clean IM bit: generate invalid operation exceptions
	cw &= ~(1 << 2);	// Set ZM bit: generate div-by-zero exceptions
	fpu::fldcw(cw);

	uint64_t cr4 = cr4_get();
	cr4 |= (1 << 9);	// Set OSFXSR bit (Enable SSE support)
	cr4 |= (1 << 10);	// Set OSXMMEXCPT bit (Enable unmasked SSE exceptions)
//	cr4 |= (1 << 18);	// Set OSXSAVE it (saves SSE registers)
	cr4_set(cr4);
}

void _start(kernel_boot_info* kbi)
{
	if (kbi->magic != KBI_MAGIC)
		panic("Bad magic number!");

	cpu_init();

	_init();

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


	{
		uint64_t a, b, c, d;
		cpuid(1, 0, a, b, c, d);

		con << "a: 0x" << hex_u64(a) << '\n';
		con << "b: 0x" << hex_u64(b) << '\n';
		con << "c: 0x" << hex_u64(c) << '\n';
		con << "d: 0x" << hex_u64(d) << '\n';
	}

	/* Unmap unused memmory */
	memory::unmap_unused();

	mallocator::init(0xffffa00000000000, 1024 * 1024 * 1024);

	/* Load TSS0 */
	ltr(TSS0);


	kmain();
	_fini();

	panic("kmain() returned!?");
}

