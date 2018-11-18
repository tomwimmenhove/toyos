#include "klib.h"
#include "debug.h"

/* Horrible unoptimized shit. */

extern "C" 
{
	void *memset(void *s, int c, size_t n);
	void *memcpy(void *dest, const void *src, size_t n);
	void *memmove(void *dest, const void *src, size_t n);
	int memcmp(const void *s1, const void *s2, size_t n);
	size_t strlen(const char *s);

	int __cxa_atexit(void (*f)(void*), void *objptr, void *dso);
	void __attribute__((noreturn)) __assert_func(const char* file, int line, const char* fn, const char* assertion);
	void __attribute__((noreturn)) __stack_chk_fail(void);
	void __cxa_pure_virtual();
};

void *memset(void *s, int c, size_t n)
{
	uint8_t* p = (uint8_t*) s;

	while (n--)
		*p++ = c;

	return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	const uint8_t* s = (const uint8_t*) src;
	uint8_t* d = (uint8_t*) dest;

	while (n--)
		*d++ = *s++;

	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	const uint8_t* s = (const uint8_t*) src;
	uint8_t* d = (uint8_t*) dest;

	if (dest < src)
		return memcpy(dest, src, n);

	for (size_t i = n; i > 0; i--)
		d[i - 1] = s[i - 1];

	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	const uint8_t* p1 = (const uint8_t*) s1;
	const uint8_t* p2 = (const uint8_t*) s2;

	while(n--)
	{
		if (*p1 < *p2)
			return -1;
		if (*p1 > *p2)
			return 1;
		p1++;
		p2++;
	}

	return 0;
}

size_t strlen(const char *s)
{
	size_t len = 0;
	while (*s++)
		len++;

	return len;
}

void __cxa_pure_virtual()
{
	panic("Virtual method called");
}

int __cxa_atexit(void (*f)(void*), void *objptr, void *dso);

int __cxa_atexit(void (*)(void*), void*, void*)
{
	/* Pffrt. There is no exit. */
	return 0;
}

void __attribute__((noreturn)) __assert_func(const char* file, int line, const char* fn, const char* assertion)
{
	con << file << ':' << line << ':' << fn << ": Assertion '" << assertion << "' failed.\n";
	panic("Assertion failed\n");
}

uintptr_t __stack_chk_guard = 0x42dead42beefface;
 
void __attribute__((noreturn)) __stack_chk_fail(void)
{
	panic("Stack smash detected");
}

