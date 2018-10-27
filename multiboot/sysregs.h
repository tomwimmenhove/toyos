#ifndef SYSREGS_H
#define SYSREGS_H

#include <stdint.h>

#define QUAUX(X) #X
#define QU(X) QUAUX(X)
#define MAKE_CR_GETSET(name)			 \
static inline uint32_t name##_get()		 \
{						 \
	uint32_t ret;				 \
	asm volatile(   "mov %%" QU(name) ", %0" \
			: "=r" (ret)		 \
			: :);			 \
	return ret;				 \
}						 \
						 \
static inline void name##_set(uint32_t cr)	 \
{						 \
	asm volatile(   "mov %0, %%" QU(name)	 \
			: : "r" (cr)		 \
			:);			 \
}

MAKE_CR_GETSET(cr0)
MAKE_CR_GETSET(cr1)
MAKE_CR_GETSET(cr2)
MAKE_CR_GETSET(cr3)
MAKE_CR_GETSET(cr4)

static inline uint32_t get_flags()
{
	uint32_t flags;
	asm volatile(   "pushf\n"
			"pop %%eax\n"
			: "=a" (flags)
			: :);
	return flags;
}


static inline uint32_t rdmsr(uint32_t msr)
{
	uint32_t ret;
	asm volatile(   "rdmsr"
			: "=a" (ret)
			: "c" (msr));
	return ret;
}

static inline void wrmsr(uint32_t msr, uint32_t x)
{
	asm volatile(   "wrmsr"
			: : "c" (msr), "a" (x)
			:);
}

#endif /* SYSREGS_H */
