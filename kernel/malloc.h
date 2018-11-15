#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>

#include "mem.h"
#include "frame_alloc.h"
#include "debug.h"

struct __attribute__ ((packed)) mallocator_chunk
{
	mallocator_chunk* prev;
	mallocator_chunk* next;

	size_t len;
	bool used;
	uint8_t __pad[0x8 - sizeof(bool)];
	uint8_t data[0];
};

class  mallocator
{
public: 
	static void init(uint64_t virt_start, size_t max_size);
	static void* malloc(size_t size);
	static void free(void* p);
	static void handle_pg_fault(interrupt_state*, uint64_t addr);
	static void test();

private:
	static mallocator_chunk* head;
	static uint64_t virt_start;
	static size_t max_size;
};

#endif /* MALLOC_H */
