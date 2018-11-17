#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

extern "C" size_t __syscall1(uint64_t a);
extern "C" size_t __syscall2(uint64_t a, uint64_t b);
extern "C" size_t __syscall3(uint64_t a, uint64_t b, uint64_t c);
extern "C" size_t __syscall4(uint64_t a, uint64_t b, uint64_t c, uint64_t d);
extern "C" size_t __syscall5(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e);
extern "C" size_t __syscall6(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f);

inline size_t syscall(uint64_t a)
{
	return __syscall1(a);
}

inline size_t syscall(uint64_t a, uint64_t b)
{
	return __syscall2(a, b);
}

inline size_t syscall(uint64_t a, uint64_t b, uint64_t c)
{
	return __syscall3(a, b, c);
}

inline size_t syscall(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
	return __syscall4(a, b, c, d);
}

inline size_t syscall(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
	return __syscall5(a, b, c, d, e);
}

inline size_t syscall(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f)
{
	return __syscall6(a, b, c, d, e, f);
}

#endif /* SYSCALL_H */

