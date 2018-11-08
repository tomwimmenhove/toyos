#include "new.h"

void *operator new(size_t size)
{
	return mallocator::malloc(size);
}

void *operator new[](size_t size)
{
	return mallocator::malloc(size);
}

void operator delete(void *p)
{
	mallocator::free(p);
}

void operator delete[](void *p)
{
	mallocator::free(p);
}

void operator delete(void*, long unsigned int)
{
}

void operator delete [](void*, long unsigned int)
{
}

