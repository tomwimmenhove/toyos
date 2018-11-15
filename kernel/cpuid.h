#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>
#include <assert.h>
#include <stdint.h>

class cpuid
{
public:
	cpuid(uint64_t in_rax, uint64_t in_rcx = 0)
	{
		assert(has_cpuid());
		get_cpuid(in_rax, in_rcx, rax, rbx, rcx, rdx);
	}

	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;

private:
	static inline bool has_cpuid()
	{
		uint64_t flags;
		uint64_t flags_id;

		asm volatile(
				/* Move FLAGS into eax */
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

	static inline void get_cpuid(uint64_t in_rax, uint64_t in_rcx, uint64_t& out_rax, uint64_t& out_rbx, uint64_t& out_rcx, uint64_t& out_rdx)
	{
		asm volatile("cpuid"
				: "=a" (out_rax), "=b" (out_rbx), "=c" (out_rcx), "=d" (out_rdx)
				: "a" (in_rax), "b" (in_rcx));
	}
};


#endif /* CPUID_H */
