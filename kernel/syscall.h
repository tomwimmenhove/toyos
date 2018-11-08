#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

extern "C" void __syscall1(uint64_t a);
extern "C" void __syscall2(uint64_t a, uint64_t b);
extern "C" void __syscall3(uint64_t a, uint64_t b, uint64_t c);
extern "C" void __syscall4(uint64_t a, uint64_t b, uint64_t c, uint64_t d);
extern "C" void __syscall5(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e);
extern "C" void __syscall6(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);

inline void syscall(uint64_t a)
{
	__syscall1(a);
}

inline void syscall(uint64_t a, uint64_t b)
{
	__syscall2(a, b);
}

inline void syscall(uint64_t a, uint64_t b, uint64_t c)
{
	__syscall3(a, b, c);
}

inline void syscall(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
	__syscall4(a, b, c, d);
}

inline void syscall(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
	__syscall5(a, b, c, d, e);
}

inline void syscall(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f)
{
	__syscall6(a, b, c, d, e, f);
}

#endif /* SYSCALL_H */

