#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>

static inline int has_cpuid()
{
	uint64_t flags;
	uint64_t flags_id;

	asm volatile(	/* Move FLAGS into eax */
			"pushf\n"
			"pop %%rax\n"

			/* Backup original flags */
			"mov %%rax, %%rcx\n"

			/* Flip ID bit */
			"xor $0x200000, %%rax\n"

			/* Move eax back into FLAGS */
			"push %%rax\n"
			"popf\n"

			/* Move FLAGS back into eax */
			"pushf\n"
			"pop %%rax\n"

			/* Restore out backup */
			"push %%rcx\n"
			"popf\n"

			: "=a" (flags), "=c" (flags_id)
			: :);

	return flags != flags_id;;
}

static inline void cpuid(uint64_t in_eax, uint64_t in_ecx, uint64_t& out_eax, uint64_t& out_ebx, uint64_t& out_ecx, uint64_t& out_edx)
{
	asm volatile(	"cpuid"
			: "=a" (out_eax), "=b" (out_ebx), "=c" (out_ecx), "=d" (out_edx)
			: "a" (in_eax), "b" (in_ecx)
			:);
}

#endif /* CPUID_H */
