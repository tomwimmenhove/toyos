#include "klib.h"
#include "debug.h"

/* Horrible unoptimized shit. */

extern "C" void *memset(void *s, int c, size_t n)
{
	uint8_t* p = (uint8_t*) s;

	while (n--)
		*p++ = c;

	return s;
}

extern "C" void *memcpy(void *dest, const void *src, size_t n)
{
	const uint8_t* s = (const uint8_t*) src;
	uint8_t* d = (uint8_t*) dest;

	while (n--)
		*d++ = *s++;

	return dest;
}

extern "C" void __cxa_pure_virtual()
{
	panic("Virtual method called");
}


