#ifndef KLIB_H
#define KLIB_H

#include <stdint.h>
#include <stddef.h>

extern "C"
{
	void *memset(void *s, int c, size_t n);
	void *memcpy(void *dest, const void *src, size_t n);
	void *memcpy(void *dest, const void *src, size_t n);
	int memcmp(const void *s1, const void *s2, size_t n);
	size_t strlen(const char *s);
};


#endif /* KLIB_H */
