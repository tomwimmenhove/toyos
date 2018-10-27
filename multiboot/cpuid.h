#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>

static inline int has_cpuid()
{
	uint32_t flags;
	uint32_t flags_id;

	asm volatile(	/* Move FLAGS into eax */
			"pushf\n"
			"pop %%eax\n"

			/* Backup original flags */
			"mov %%eax, %%ecx\n"

			/* Flip ID bit */
			"xor $0x200000, %%eax\n"

			/* Move eax back into FLAGS */
			"push %%eax\n"
			"popf\n"

			/* Move FLAGS back into eax */
			"pushf\n"
			"pop %%eax\n"

			/* Restore out backup */
			"push %%ecx\n"
			"popf\n"

			: "=a" (flags), "=c" (flags_id)
			: :);

	return flags != flags_id;;
}

static inline void cpuid(uint32_t in_eax, uint32_t in_ecx, uint32_t* out_eax, uint32_t* out_ebx, uint32_t* out_ecx, uint32_t* out_edx)
{
	asm volatile(	"cpuid"
			: "=a" (*out_eax), "=b" (*out_ebx), "=c" (*out_ecx), "=d" (*out_edx)
			: "a" (in_eax), "b" (in_ecx)
			:);
}

#endif /* CPUID_H */
