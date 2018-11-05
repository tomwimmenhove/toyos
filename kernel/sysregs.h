#ifndef SYSREGS_H
#define SYSREGS_H

#include <stdint.h>

#define QUAUX(X) #X
#define QU(X) QUAUX(X)
#define MAKE_CR_GETSET(name)					\
static inline uint64_t name##_get()				\
{						 						\
	uint64_t ret;				 				\
	asm volatile(   "mov %%" QU(name) ", %0"	\
			: "=r" (ret)						\
			: :);			 					\
	return ret;				 					\
}						 						\
												\
static inline void name##_set(uint64_t cr)	 	\
{						 						\
	asm volatile(   "mov %0, %%" QU(name)	 	\
			: : "r" (cr)		 				\
			:);			 						\
}

MAKE_CR_GETSET(cr0)
MAKE_CR_GETSET(cr1)
MAKE_CR_GETSET(cr2)
MAKE_CR_GETSET(cr3)
MAKE_CR_GETSET(cr4)

#endif /* SYSREGS_H */
