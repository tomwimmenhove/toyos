#include <stdint.h>
#include <stddef.h>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/config.h"
}

#include "new.h"
#include "mb.h"
#include "memory.h"
#include "frame_alloc.h"

extern void* _data_end;
extern void* _code_start;

void die()
{
	asm volatile("movl $0, %eax");
	asm volatile("out %eax, $0xf4");
	asm volatile("cli");
	for (;;)
	{
		asm volatile("hlt");
	}
}

extern "C" void __cxa_pure_virtual()
{
	putstring("Virtual method called\n");
	die();
}

unsigned char answer = 65;

void print_stack_use()
{
	putstring("stack usuage: ");
	put_hex_long(KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0));
	put_char('\n');
}

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
	mallocator(memory* mem, uint64_t virt_start)
		: mem(mem), virt_start(virt_start)
	{
		mem->map_page(virt_start, mem->frame_alloc->page());

		head = (mallocator_chunk*) virt_start;

		head->next = head;
		head->prev = head;
		head->used = 0;
		head->len = 0x1000 - sizeof(mallocator_chunk);
	}

	void* malloc(size_t size)
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
			mem->map_page(end_virt, mem->frame_alloc->page());
			end_virt += 0x1000;
		}

		tail->len += size;

		return malloc(size);
	}

	void free(void* p)
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

		putstring("Trying to free a non-existing chunk of memory!\n");
		die();
	}

	void test()
	{
		int ia = 1234;
		int ib = 4321;
		int ic = 10000;
		int id = 16;
		int ie = 5555;

		uint8_t* a = (uint8_t*) malloc(ia);
		putstring("Malloc a: "); put_hex_long((uint64_t) a); put_char('\n');
		uint8_t* b = (uint8_t*) malloc(ib);
		putstring("Malloc b: "); put_hex_long((uint64_t) b); put_char('\n');
		uint8_t* c = (uint8_t*) malloc(ic);
		putstring("Malloc c: "); put_hex_long((uint64_t) c); put_char('\n');
		uint8_t* d = (uint8_t*) malloc(id);
		putstring("Malloc d: "); put_hex_long((uint64_t) d); put_char('\n');
		uint8_t* e = (uint8_t*) malloc(ie);
		putstring("Malloc e: "); put_hex_long((uint64_t) e); put_char('\n');

		free(c);
		c = (uint8_t*) malloc(ic);
		putstring("Malloc c: "); put_hex_long((uint64_t) c); put_char('\n');

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
			{ putstring("aDeath\n"); die(); }

		for (int i = 0; i < ib; i++)
			if (b[i] != (uint8_t) i)
			{ putstring("bDeath\n"); die(); }

		for (int i = 0; i < ic; i++)
			if (c[i] != (uint8_t) i)
			{ putstring("cDeath\n"); die(); }

		for (int i = 0; i < id; i++)
			if (d[i] != (uint8_t) i)
			{ putstring("dDeath\n"); die(); }

		for (int i = 0; i < ie; i++)
			if (e[i] != (uint8_t) i)
			{ putstring("eDeath\n"); die(); }
	}

private:
	mallocator_chunk* head;
	memory* mem;
	uint64_t virt_start;
};

int kmain(kernel_boot_info* kbi)
{
	(void) kbi;
	mallocator malor(mem, 0xffffa00000000000);

	malor.test();

#if 0
	// XXX: Just a test!
	uint8_t* ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 126 * 1024 * 1024; i += 0x1000)
	{ 
		mem.map_page((uint64_t) ppp, mem.frame_alloc->page());
		ppp += 0x1000;
	} 
	uint64_t* pppp = (uint64_t*) 0xffffa00000000000;
	for (uint64_t i = 0; i < 126 * 1024 * 1024 / sizeof(uint64_t); i++)
		pppp[i] = 0x41414141;
#endif

	uint64_t phys = 0xb8000;
	uint64_t virt = 0xffffffff40000000 - 0x1000;

	mem->map_page(virt, phys);

	volatile unsigned char* pp = (unsigned char*) virt;
	for (int i = 0; i < 4096; i++)
	{
		pp[i] = 65;
	}

	for(;;)asm("hlt");


	die();
}

extern "C" {
	void _init();
	void _fini();

	void _start(kernel_boot_info* kbi);
}
void _start(kernel_boot_info* kbi)
{
	if (kbi->magic != KBI_MAGIC)
	{
		putstring("Bad magic number: ");
		put_hex_int(kbi->magic); put_char('\n');
		die();
	}

	memory mem(kbi);
	memory::init(&mem);

	_init();

	kmain(kbi);

	_fini();
}

