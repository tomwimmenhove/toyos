#include <stdint.h>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/multiboot2.h"
}

extern void* _data_end;
uint8_t* last_page;

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

void map_page(uint64_t virt, uint64_t phys)
{
	virt &= ~0xfff;
	phys &= ~0xfff;

	uint64_t pml4e = (virt >> 39) & 511;
	uint64_t pdpe = (virt >> 30) & 511;
	uint64_t pdee = (virt >> 21) & 511;
	uint64_t pte = (virt >> 12) & 511;

	/* No longer at pml4[511], but at 510 */
#if 0
	uint64_t* pml4 = (uint64_t*) (0xfffffffffffff000llu);
	uint64_t* pdp = (uint64_t*) (0xffffffffffe00000llu | ((virt >> 27) & 0x00000000001ff000ull) );
	uint64_t* pde = (uint64_t*) (0xffffffffc0000000llu | ((virt >> 18) & 0x000000003ffff000ull) );
	uint64_t* pt = (uint64_t*) (0xffffff8000000000llu | ((virt >>  9) & 0x0000007ffffff000ull) );
#else
	/* So now we can't simply have all the upper bits high, but instead the indices will be 510, so
	 *  0b1111111111111111 111111111 111111111 111111111 111111111 000000000000 // 0xfffffffffffff000
	 * Becomes:
	 *  0b1111111111111111 111111110 111111110 111111110 111111110 000000000000 // 0xffffff7fbfdfe000
	 *  ... etc
	 */
	uint64_t* pml4 = (uint64_t*) (0xffffff7fbfdfe000llu);
	uint64_t* pdp = (uint64_t*) (0xffffff7fbfc00000llu | ((virt >> 27) & 0x00000000001ff000ull) );
	uint64_t* pde = (uint64_t*) (0xffffff7f80000000llu | ((virt >> 18) & 0x000000003ffff000ull) );
	uint64_t* pt = (uint64_t*) (0xffffff0000000000llu | ((virt >>  9) & 0x0000007ffffff000ull) );
#endif

	if (!(pml4[pml4e] & 1))
	{
		pml4[pml4e] = (uint64_t) new_page() | 3;
		clear_page((void*) pdp);
	}

	if (!(pdp[pdpe] & 1))
	{
		pdp[pdpe] = (uint64_t) new_page() | 3;
		clear_page((void*) pde);
	}

	if (!(pde[pdee] & 1))
	{
		pde[pdee] = (uint64_t) new_page() | 3;
		clear_page((void*) pt);
	}

	pt[pte] = phys | 3;
}

void clean_page_tables()
{
	uint64_t* pml4 = (uint64_t*) 0xffffff7fbfdfe000llu;

	/* We can simply unmap everything in the lower half */
	for (int i = 0; i < 0x100; i++)
		pml4[i] = 0;

	/* Burtally invalidate all TLB cache */
	cr3_set(cr3_get());
}

int kmain(struct multiboot_tag* mb_tag)
{
	putstring("Multiboot structure at ");
	put_hex_long((uint64_t) mb_tag);
	put_char('\n');

	clean_page_tables();

	uint64_t phys = 0xb8000;
	uint64_t virt = 0xffff900000000000;

	map_page(virt, phys);

	volatile unsigned char* p = (unsigned char*) virt;
	for (int i = 0; i < 4096; i++)
	{
		p[i] = answer;
	}


	for (;;)
		asm("hlt");

}

extern "C" void _start(struct multiboot_tag* mb_tag)
{
	last_page = (uint8_t*) 0x1000000; // XXX: FIX THIS
	kmain(mb_tag);
}

