#ifndef FPU_H
#define FPU_H

#include <stdint.h>

class fpu
{
public:
	static inline uint16_t fstcw()
	{
		uint16_t r;

		asm volatile("fstcw %0;":"=m"(r));

		return r;
	}

	static inline void fninit() { asm volatile("fninit"); }

	static inline void fldcw(uint16_t control) { asm volatile("fldcw %0;"::"m"(control)); }
};

#endif /* FPU_H */
