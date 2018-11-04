#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>

#include "memory.h"
#include "frame_alloc.h"
#include "debug.h"

struct mallocator_chunk
{
	mallocator_chunk* prev;
	mallocator_chunk* next;

	size_t len;
	int used;
	uint8_t data[0];
};

class mallocator
{
public: 
	mallocator(uint64_t virt_start);
	void* malloc(size_t size);
	void free(void* p);
	void test();

private:
	mallocator_chunk* head;
	uint64_t virt_start;
};

#endif /* MALLOC_H */
