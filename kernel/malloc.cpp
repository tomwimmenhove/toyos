#include "malloc.h"

mallocator::mallocator(uint64_t virt_start)
	: virt_start(virt_start)
{
	memory::map_page(virt_start, memory::frame_alloc->page());

	head = (mallocator_chunk*) virt_start;

	head->next = head;
	head->prev = head;
	head->used = 0;
	head->len = 0x1000 - sizeof(mallocator_chunk);
}

void* mallocator::malloc(size_t size)
{
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

				new_chunk->used = 0;
				new_chunk->len = chunk->len - size - sizeof(mallocator_chunk);

				new_chunk->prev = chunk;
				new_chunk->next = chunk->next;

				chunk->next->prev = new_chunk;
				chunk->next = new_chunk;

				chunk->len = size;
			}

			chunk->used = 1;
			return chunk->data;
		}
		chunk = chunk->next;
	} while (chunk != head->prev);

	/* Allocate more */
	auto tail = head->prev;
	uint64_t end_virt = (((uint64_t) &tail->data[tail->len - 1]) & ~0xfff) + 0x1000;
	for (size_t i = 0; i < size; i += 0x1000)
	{
		memory::map_page(end_virt, memory::frame_alloc->page());
		end_virt += 0x1000;
	}

	tail->len += size;

	return malloc(size);
}

void mallocator::free(void* p)
{
	auto chunk = head;
	do
	{
		if (p == chunk->data)
		{
			if (!chunk->prev->used)
			{
				/* Combine with the previous chunk */
				chunk->prev->len += chunk->len + sizeof(mallocator_chunk);
				chunk->prev->next = chunk->next;
				return;
			}

			chunk->used = 0;

			if (!chunk->next->used)
			{
				/* Combine with the next chunk */
				chunk->len += chunk->next->len + sizeof(mallocator_chunk);
				chunk->next = chunk->next->next;
			}
			return;
		}
		chunk = chunk->next;
	} while (chunk != head);

	panic("Trying to free a non-existing chunk of memory!");
	die();
}

void mallocator::test()
{
	int ia = 1234;
	int ib = 4321;
	int ic = 10000;
	int id = 16;
	int ie = 5555;

	uint8_t* a = (uint8_t*) malloc(ia);
	dbg << "Malloc a: " << ((uint64_t) a) << '\n';
	uint8_t* b = (uint8_t*) malloc(ib);
	dbg << "Malloc b: " << ((uint64_t) b) << '\n';
	uint8_t* c = (uint8_t*) malloc(ic);
	dbg << "Malloc c: " << ((uint64_t) c) << '\n';
	uint8_t* d = (uint8_t*) malloc(id);
	dbg << "Malloc d: " << ((uint64_t) d) << '\n';
	uint8_t* e = (uint8_t*) malloc(ie);

	free(c);
	c = (uint8_t*) malloc(ic);
	dbg << "Malloc c: " << ((uint64_t) c) << '\n';

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

