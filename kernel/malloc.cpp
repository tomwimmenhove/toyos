#include <assert.h>

#include "malloc.h"
#include "config.h"

mallocator_chunk* mallocator::head;
uint64_t mallocator::virt_start;
uint64_t mallocator::max_size;

void mallocator::init(uint64_t virt_start, size_t max_size)
{
	mallocator::virt_start = virt_start;
	mallocator::max_size = max_size;

	head = (mallocator_chunk*) virt_start;

	head->next = head;
	head->prev = head;
	head->used = 0;
	head->len = PAGE_SIZE - sizeof(mallocator_chunk);
}

void* mallocator::malloc(size_t size)
{
	/* Align */
	size = (size + 0xf) & ~0xf;

	/* Find a block that's not used with at least enough space. Unless it's the last block, in which
	 * case we can 'expand' it */
	auto chunk = head->prev;
	do
	{
		if (!chunk->used && chunk->len >= size)
		{
			/* Room for a new one? */
			if (chunk->len - size > sizeof(mallocator_chunk))
			{
				mallocator_chunk* new_chunk = (mallocator_chunk*) &chunk->data[size];

				new_chunk->used = false;
				new_chunk->len = chunk->len - size - sizeof(mallocator_chunk);
				new_chunk->magic = ~mallocator_chunk::MAGIC; // For testing. Won't hurt.

				new_chunk->prev = chunk;
				new_chunk->next = chunk->next;

				chunk->next->prev = new_chunk;
				chunk->next = new_chunk;

				chunk->len = size;
			}

			chunk->used = true;
			chunk->magic = mallocator_chunk::MAGIC;

			return chunk->data;
		}
		chunk = chunk->next;
	} while (chunk != head->prev);

	/* Allocate more */
	auto tail = head->prev;
	if (!tail->used)
	{
		tail->used = true;
		tail->len = size;
		tail->magic = mallocator_chunk::MAGIC;

		return tail->data;
	}
	else
	{
		/* add a new chunk at the end */
		mallocator_chunk* new_chunk = (mallocator_chunk*) &tail->data[tail->len];
		new_chunk-> len = size;
		new_chunk->used = true;
		new_chunk->magic = mallocator_chunk::MAGIC;

		new_chunk->next = head;
		new_chunk->prev = tail;
		head->prev = new_chunk;
		tail->next = new_chunk;

		return new_chunk->data;
	}
}

void mallocator::free(void* p)
{
	mallocator_chunk* chunk = (mallocator_chunk*) ((uintptr_t) p - (uintptr_t) &((mallocator_chunk*) 0)->data);

	/* This seems redundant, but it allows us to detect double-frees.
	 * If the first assert fails -> double free
	 * If the second assert fails -> corrupted memory or bad pointer. */
	assert(chunk->magic != ~mallocator_chunk::MAGIC);
	assert(chunk->magic == mallocator_chunk::MAGIC);
	if (!chunk->prev->used)
	{
		/* Combine with the previous chunk */
		chunk->prev->len += chunk->len + sizeof(mallocator_chunk);
		chunk->next->prev = chunk->prev;
		chunk->prev->next = chunk->next;

		chunk = chunk->prev;
	}

	chunk->used = false;
	chunk->magic = ~mallocator_chunk::MAGIC;

	if (!chunk->next->used)
	{
		assert(chunk->next->magic == ~mallocator_chunk::MAGIC);
		/* Combine with the next chunk */
		chunk->len += chunk->next->len + sizeof(mallocator_chunk);
		chunk->next->next->prev = chunk;
		chunk->next = chunk->next->next;
	}

	/* Unmap */
	uint64_t start = ((uint64_t) chunk->data + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	uint64_t stop = ((uint64_t) chunk->data + chunk->len) & ~(PAGE_SIZE - 1);
	for (auto addr = start; addr < stop; addr += PAGE_SIZE)
	{
		auto phys = memory::is_mapped(addr);
		if (phys)
		{
			memory::unmap_page(addr);
			memory::frame_alloc->free(phys);
		}
	}
}

bool mallocator::handle_pg_fault(interrupt_state* state, uint64_t addr)
{
	/* Check if the page was present. If it was, it's not our problem */
	if (state->err_code & 1)
		return false;

	/* Map the page if it's our's */
	if (addr >= virt_start && addr < virt_start + max_size)
	{
		memory::map_page(addr & ~(PAGE_SIZE - 1), memory::frame_alloc->page());

		return true;
	}

	return false;
}

void mallocator::test()
{
	int ia = 1234;
	int ib = 4321;
	int ic = 10000;
	int id = 16;
	int ie = 5555;

	uint8_t* a = (uint8_t*) malloc(ia);
	uint8_t* b = (uint8_t*) malloc(ib);
	uint8_t* c = (uint8_t*) malloc(ic);
	uint8_t* d = (uint8_t*) malloc(id);
	uint8_t* e = (uint8_t*) malloc(ie);

	free(c);
	c = (uint8_t*) malloc(ic);

	for (int i = 0; i < ia; i++)
		a[i] = i;
	for (int i = 0; i < ib; i++)
		b[i] = i;
	for (int i = 0; i < ic; i++)
		c[i] = i;
	for (int i = 0; i < id; i++)
		d[i] = i;
	for (int i = 0; i < ie; i++)
		e[i] = i;

	for (int i = 0; i < ia; i++)
		if (a[i] != (uint8_t) i)
			panic("CORRUPTION");

	for (int i = 0; i < ib; i++)
		if (b[i] != (uint8_t) i)
			panic("CORRUPTION");

	for (int i = 0; i < ic; i++)
		if (c[i] != (uint8_t) i)
			panic("CORRUPTION");

	for (int i = 0; i < id; i++)
		if (d[i] != (uint8_t) i)
			panic("CORRUPTION");

	for (int i = 0; i < ie; i++)
		if (e[i] != (uint8_t) i)
			panic("CORRUPTION");
}

