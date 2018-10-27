#include <stdint.h>

#include "debug_out.h"

extern void* _data_end;
uint8_t* last_page;;

void* new_page()
{
	last_page += 0x1000;
	return last_page;
}

void clear_page(void* page)
{
	uint64_t* p = (uint64_t*) page;

	for (int i = 0; i < 512; i++)
	{
		p[i] = 0;
	}
}

static inline uint64_t cr3_get()
{
	uint64_t ret;
	asm volatile(   "mov %%cr3 , %0"
			: "=r" (ret)
			: :);
	return ret;
}

static inline void cr3_set(uint64_t cr)
{
	asm volatile(   "mov %0, %%cr3"
			: : "r" (cr)
			:);
}



extern "C" void __cxa_pure_virtual()
{
	// Do nothing or print an error message.
}

unsigned char answer = 65;

void map_page(uint64_t virt_addr, uint64_t phys_addr)
{
	virt_addr &= ~0xfff;
	phys_addr &= ~0xfff;

	uint64_t p4e = (virt_addr >> 39) & 511;
	uint64_t p3e = (virt_addr >> 30) & 511;
	uint64_t p2e = (virt_addr >> 21) & 511;
	uint64_t p1e = (virt_addr >> 12) & 511;

	uint64_t* p4 = (uint64_t*) (0xfffffffffffff000llu);
	uint64_t* p3 = (uint64_t*) (0xffffffffffe00000llu | ((virt_addr >> 27) & 0x00000000001ff000ull) );
	uint64_t* p2 = (uint64_t*) (0xffffffffc0000000llu | ((virt_addr >> 18) & 0x000000003ffff000ull) );
	uint64_t* p1 = (uint64_t*) (0xffffff8000000000llu | ((virt_addr >>  9) & 0x0000007ffffff000ull) );

	if (!(p4[p4e] & 1))
	{
		p4[p4e] = (uint64_t) new_page() | 3;
		clear_page((void*) p3);
	}

	if (!(p3[p3e] & 1))
	{
		p3[p3e] = (uint64_t) new_page() | 3;
		clear_page((void*) p2);
	}

	if (!(p2[p2e] & 1))
	{
		p2[p2e] = (uint64_t) new_page() | 3;
		clear_page((void*) p1);
	}

	p1[p1e] = phys_addr | 3;
}

int main()
{
	uint64_t phys = 0xb8000;

//	uint64_t virt = 0x123873947000ull;
	uint64_t virt = 0xffff800000000000;

	map_page(virt, phys);

	volatile unsigned char* p = (unsigned char*) virt;

	for (int i = 0; i < 4096; i++)
	{
		p[i] = answer;
	}


	for (;;)
		asm("hlt");

}

extern "C" void _start()
{
	last_page = (uint8_t*) (((uint64_t) &_data_end) & ~0xfff);
//	last_page = (uint8_t*) 0x20000000;
	main();
}

