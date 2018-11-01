#include <cstdint>
#include <cstddef>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/multiboot2.h"
	#include "../common/config.h"
}

extern void* _data_end;
extern void* _code_start;
static uint64_t alloc_ptr;

static kernel_boot_info* kbi;

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

uint64_t get_phys(uint64_t virt)
{
	virt &= 0xffffffffffff;

	uint64_t* pt = (uint64_t*) PG_PT;
	uint64_t pte = virt >> 12;

	return (pt[pte] & ~0xfff) | (virt & 0xfff);
}

uint64_t new_page()
{
	uint64_t p = alloc_ptr;
	alloc_ptr += 0x1000;
	while  ( 	(p >= kbi->alloc_first && p < kbi->alloc_end) ||
			(p >= get_phys((uint64_t) &_code_start) && p <= get_phys((uint64_t) &_data_end - 0x1000)))

	{
		alloc_ptr += 0x1000;
		p += 0x1000;
	}

	// XXX: Dude!
	return p;
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
	uint64_t pde = (virt >> 21) & 511;
	uint64_t pte = (virt >> 12) & 511;

	uint64_t* pml4 = (uint64_t*) (PG_PML4);
	uint64_t* pdp = (uint64_t*) (PG_PDP | ((virt >> 27) & 0x00000000001ff000ull) );
	uint64_t* pd = (uint64_t*) (PG_PD | ((virt >> 18) & 0x000000003ffff000ull) );
	uint64_t* pt = (uint64_t*) (PG_PT | ((virt >>  9) & 0x0000007ffffff000ull) );

	if (!(pml4[pml4e] & 1))
	{
		pml4[pml4e] = (uint64_t) new_page() | 3;
		clear_page((void*) pdp);
	}

	if (!(pdp[pdpe] & 1))
	{
		pdp[pdpe] = (uint64_t) new_page() | 3;
		clear_page((void*) pd);
	}

	if (!(pd[pde] & 1))
	{
		pd[pde] = (uint64_t) new_page() | 3;
		clear_page((void*) pt);
	}

	pt[pte] = phys | 3;
}

void map_pages(uint64_t virt, uint64_t phys, uint64_t size)
{
	if (size & 0xfff)
	{
		putstring("Trying to map a non-page-aligned size.\n");
		die();
	}
	for (uint64_t i = 0; i < size; i += 0x1000, virt += 0x1000, phys += 0x1000)
	{
		map_page(virt, phys);
	}
}


void clean_page_tables()
{
	uint64_t* pml4 = (uint64_t*) PG_PML4;
	for (int i = 0; i < 0x200; i++)
	{
		if (pml4[i] & 1)
		{
			uint64_t* pdp = (uint64_t*) (pml4[i] & ~0xfff);
			for (int j = 0; j < 0x200; j++)
			{
				if (pdp[j] & 1)
				{
					uint64_t* pd = (uint64_t*) (pdp[j] & ~0xfff);
					for (int k = 0; k < 0x200; k++)
					{
						if (pd[k] & 1)
						{
							uint64_t* pt = (uint64_t*) (pd[k] & ~0xfff);
							for (int l = 0; l < 0x200; l++)
							{
								if (pt[l] & 1)
								{
									uint64_t virt =
										((uint64_t) i) << 39 |
										((uint64_t) j) << 30 |
										((uint64_t) k) << 21 |
										((uint64_t) l) << 12;
									if (i >= 0x100)
										virt |= 0xffff000000000000;
									putstring("Virt ");
									put_hex_long(virt);
									putstring(" is mapped to to ");
									put_hex_long(pt[l] & ~ 0xfff);
									put_char('\n');
								}
							}
						}
					}
				}
			}
		}
	}

	/* We can simply unmap everything in the lower half */
	for (int i = 0; i < 0x100; i++)
		pml4[i] = 0;

	/* Burtally invalidate all TLB cache */
	cr3_set(cr3_get());
}

inline void *operator new(size_t, void *p)     throw() { return p; }
inline void *operator new[](size_t, void *p)   throw() { return p; }
inline void  operator delete  (void *, void *) throw() { };
inline void  operator delete[](void *, void *) throw() { };

struct bitmap
{
	bitmap(size_t size)
		: size(size)
	{ }

	inline int at(size_t p)
	{
		check_index(p);
		return data[p / 8] & (1 << (p % 8));
	}

	inline void set(size_t p)
	{
		check_index(p);
		data[p / 8] |= 1 << (p % 8);
	}

	inline void set_range(size_t begin, size_t end)
	{
		check_index(begin);
		check_index(end - 1);
		for (auto i = begin; i < end; i++)
			set(i);
	}

	inline void reset(size_t p)
	{
		check_index(p);
		data[p / 8] &= ~(1 << (p % 8));
	}

	inline void reset_range(size_t begin, size_t end)
	{
		check_index(begin);
		check_index(end - 1);
		for (auto i = begin; i < end; i++)
			reset(i);
	}

	inline size_t find_zero()
	{
		for (size_t i = 0 ; i < size; i++)
			if (!at(i))
				return i;
		return SIZE_MAX;
	}

private:
	inline void check_index(size_t p)
	{
		if (p >= size)
		{
			putstring("Tried to access past end of bitmap!\n");
			die();
		}
	}

private:
	size_t size;
	uint8_t data[0];
};

template<typename T, typename U>
static inline T mb_next(T entry, U tag)
{
	return (T) ((uint64_t) entry + (((U) tag)->entry_size));
}

void parse_memmap(kernel_boot_info* kbi, struct multiboot_tag_mmap* mb_mmap_tag)
{
	struct multiboot_mmap_entry *entry;
	auto entry_end = (struct multiboot_mmap_entry*) ((uint64_t) mb_mmap_tag + mb_mmap_tag->size);

	uint64_t top = 0;
	uint8_t* largest_region_ptr = nullptr;
	uint64_t largest_region_len = 0;

	uint64_t total_ram = 0;

	/* Iterate once to get the highest memory address */
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb_next(entry, mb_mmap_tag))
	{
		putstring("Memory region ");
		put_hex_long(entry->addr);
		putstring(" with length: ");
		put_hex_long(entry->len);
		putstring(" of type ");
		const char *typeNames[] = { "Unknown", "available", "reserved", "ACPI reclaimable", "NVS", "\"bad ram\""};

		putstring(typeNames[entry->type]);
		put_char('\n');

		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			total_ram += entry->len;
			top = entry->addr + entry->len;
			if (entry->len > largest_region_len)
			{
				largest_region_ptr = (uint8_t*) entry->addr;
				largest_region_len = entry->len;
			}
		}
	}

	putstring("Top of ram ");
	put_hex_long(top);
	put_char('\n');
	putstring("Total ram ");
	put_hex_long(total_ram);
	put_char('\n');


	if (!largest_region_len)
	{
		putstring("No space found for physical memory page bitmap\n");
		die();
	}

	putstring("Largest region at "); put_hex_long((uint64_t) largest_region_ptr); putstring(" - "); put_hex_long((uint64_t)largest_region_ptr + largest_region_len); put_char('\n');

	alloc_ptr = (uint64_t) largest_region_ptr;
	auto end_page = largest_region_ptr + largest_region_len;

	/* We need one bit per page */
	uint64_t bitmap_size = ((top + 0xfff) / 0x1000 + 7) / 8;
	uint8_t* bitmapd = (uint8_t*) 0xffffffff40000000;

	putstring("Needs a bitmap of ");
	put_hex_short(bitmap_size);
	put_char('\n');

	/* Lets allocate a bunch, we're just going to start allocating at the largest region and hope it's big enough */
	uint64_t allocced = 0;
	uint8_t* p = (uint8_t*) bitmapd;
	while (allocced < bitmap_size + sizeof(bitmap)) // Don't forget the size of the bitmap struct itself
	{
		auto pg = new_page();

		if ((uint8_t*) pg >= end_page)
		{
			putstring("Ran out of memory for storing physical memory page bitmap\n");
			die();
		}

		map_page((uint64_t) p, (uint64_t) pg);
		p += 0x1000;
		allocced += 0x1000;
	}

	putstring("writing some test shit\n");
	for (uint64_t i = 0; i < bitmap_size; i++)
		bitmapd[i] = i;
	putstring("reading the test shit\n");
	for (uint64_t i = 0; i < bitmap_size; i++)
	{
		if (bitmapd[i] != (uint8_t) i)
		{
			putstring("FAILED!\n");
			die();
		}
	}

	/* Map pages for the bitmap (including the struct itself) into memory */
//	map_pages((uint64_t) bitmapd, kbi->alloc_end, (bitmap_size + sizeof(bitmap) + 0xfff) & ~0xfff);

	auto bm = new(reinterpret_cast<void*>(bitmapd)) bitmap(top / 0x1000);

	bm->set_range(0, top / 0x1000);

	return;

	/* Iterate over the memory map again, and set all the bits in the bitmap */
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb_next(entry, mb_mmap_tag))
	{
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			uint64_t start = (entry->addr + 0xfff) & ~0xfff;
			uint64_t end = (entry->addr + entry->len) & ~0xfff;

			if (end > start)
				for (auto i = start; i < end; i+= 0x1000)
					bm->reset(i / 0x1000);
		}
	}

	bm->reset_range(0, top / 0x1000);

	(void)kbi;
}

void parse_kbi()
{
	auto mb_tag_addr = kbi->mb_tag;
	auto mb_tag = (struct multiboot_tag *) mb_tag_addr;
	while (mb_tag->type)
	{
		mb_tag_addr += (mb_tag->size + 7) & ~7;
		mb_tag = (struct multiboot_tag *) mb_tag_addr;

		if (mb_tag->type == MULTIBOOT_TAG_TYPE_MMAP)
		{
			parse_memmap(kbi, (struct multiboot_tag_mmap*) mb_tag);
			putstring("YUP\n");
		}
	}
	die();
}

int kmain()
{
	putstring("Kernel info structure at ");
	put_hex_long((uint64_t) kbi);
	put_char('\n');

	if (kbi->magic != KBI_MAGIC)
	{
		putstring("Bad magic number: ");
		put_hex_int(kbi->magic); put_char('\n');
		die();
	}

	parse_kbi();

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

extern "C" void _start(kernel_boot_info* _kbi)
{
	kbi = _kbi;
//	alloc_ptr =  0x1000000; // XXX: FIX THIS
	kmain();
}

