#include "klib.h"
#include "debug.h"
#include "malloc.h"

/* Horrible unoptimized shit. */

extern "C" 
{
	int __cxa_atexit(void (*f)(void*), void *objptr, void *dso);
	void __attribute__((noreturn)) __assert_func(const char* file, int line, const char* fn, const char* assertion);
	void __attribute__((noreturn)) __stack_chk_fail(void);
	void __cxa_pure_virtual();
};

namespace std
{
	void __throw_bad_alloc()
	{
		panic("__throw_bad_alloc() called");
	}

	void __throw_bad_function_call()
	{
		panic("__throw_bad_function_call() callde");
	}
}

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

int strncmp(const char *s1, const char *s2, size_t n)
{
	while (*s1 && *s2 && n--)
	{
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;

		s1++;
		s2++;
	}

	if (*s1 < *s2)
		return -1;
	if (*s1 > *s2)
		return 1;

	return 0;
}

int strcmp(const char *s1, const char *s2)
{
	return strncmp(s1, s2, 0xffffffffffffffff);
}

char *strdup(const char *s)
{
	auto len = strlen(s);
	char* r = (char*) mallocator::malloc(len + 1);
	memcpy((void*) r, (void*) s, len + 1);

	return r;
}

char *index(const char *s, int c)
{
	while (*s)
	{
		if (*s == c)
			return (char*) s;
		s++;
	}

	if (c == 0)
		return (char*) s;

	return nullptr;
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

