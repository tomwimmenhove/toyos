#ifndef JUMP_H
#define JUMP_H

#include <stdint.h>

static inline void set_data_segs(uint16_t ds)
{
	asm volatile(   "mov %0, %%ds\n"
			"mov %0, %%es\n"
			"mov %0, %%fs\n"
			"mov %0, %%gs\n"
			"mov %0, %%ss\n"
			: 
			: "r" (ds));
}

static inline void far_jmp(uint16_t cs, uint32_t address)
{
	uint32_t d[2] = { address, cs };
	asm volatile(   "ljmp *(%%eax)"
			:
			: "a" (d)
			: "memory" /* Thanks to geist on #osdev on freenode */);
}

static inline void jump_kernel(uint64_t entry)
{
	asm volatile(   /* Jump to our new code segment */
			"jmp $0x8, $jump64%=\n"
			".code64\n"
			"jump64%=:\n"

			/* We're now in long mode. Load the entry point into %rax */
			"shl $0x20, %%rax\n"
			"or %%rbx, %%rax\n"

			/* Jump to it! */
			"jmp *%%rax\n"
			".code32\n"
			: : "a" ((uint32_t) (entry >> 32)), "b" ((uint32_t) (entry & 0xffffffff)));
}

#endif /* JUMP_H */
