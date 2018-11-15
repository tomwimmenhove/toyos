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

extern "C" int __cxa_atexit(void (*f)(void*), void *objptr, void *dso);

extern "C" int __cxa_atexit(void (*)(void*), void*, void*)
{
	/* Pffrt. There is no exit. */
	return 0;
}

extern "C" void __attribute__((noreturn)) __assert_func(const char* file, int line, const char* fn, const char* assertion)
{
	con << file << ':' << line << ':' << fn << ": Assertion '" << assertion << "' failed.\n";
	panic("Assertion failed\n");
}
