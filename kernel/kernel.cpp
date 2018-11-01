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

class frame_alloc_iface
{
public:
	uint64_t virtual page() = 0;
};

class new_page_dumb : public frame_alloc_iface
{
public:
	new_page_dumb(kernel_boot_info* kbi, uint64_t alloc_ptr_start, uint64_t alloc_ptr_end)
		: kbi(kbi), alloc_ptr(alloc_ptr_start), alloc_ptr_end(alloc_ptr_end)
	{ }

	uint64_t page() override
	{
		uint64_t p = alloc_ptr;
		while ((p >= kbi->alloc_first && p < kbi->alloc_end) ||
		       (p >= get_phys((uint64_t) &_code_start) && p <= get_phys((uint64_t) &_data_end - 0x1000)))
			p += 0x1000;

		alloc_ptr = p + 0x1000;

		if (p >= alloc_ptr_end)
			return 0;

		return p;
	}

private:
	kernel_boot_info* kbi;
	uint64_t alloc_ptr;
	uint64_t alloc_ptr_end;
};

uint64_t new_page()
{
	uint64_t p = alloc_ptr;
	while  (	(p >= kbi->alloc_first && p < kbi->alloc_end) ||
	       		(p >= get_phys((uint64_t) &_code_start) && p <= get_phys((uint64_t) &_data_end - 0x1000)))
		p += 0x1000;

	alloc_ptr = p + 0x1000;

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

void map_page(uint64_t virt, uint64_t phys, frame_alloc_iface* fai)
{
	virt &= ~0xfff;
	phys &= ~0xfff;

	uint64_t pml4e = (virt >> 39) & 511;
	uint64_t pdpe = (virt >> 30) & 511;
	uint64_t pde = (virt >> 21) & 511;
	uint64_t pte = (virt >> 12) & 511;

	volatile uint64_t* pml4 = (uint64_t*) (PG_PML4);
	volatile uint64_t* pdp = (uint64_t*) (PG_PDP | ((virt >> 27) & 0x00000000001ff000ull) );
	volatile uint64_t* pd = (uint64_t*) (PG_PD | ((virt >> 18) & 0x000000003ffff000ull) );
	volatile uint64_t* pt = (uint64_t*) (PG_PT | ((virt >>  9) & 0x0000007ffffff000ull) );

	if (!(pml4[pml4e] & 1))
	{
		pml4[pml4e] = (uint64_t) fai->page() | 3;
		clear_page((void*) pdp);
	}

	if (!(pdp[pdpe] & 1))
	{
		pdp[pdpe] = (uint64_t) fai->page() | 3;
		clear_page((void*) pd);
	}

	if (!(pd[pde] & 1))
	{
		pd[pde] = (uint64_t) fai->page() | 3;
		clear_page((void*) pt);
	}

	pt[pte] = phys | 3;
}

void unmap_page(uint64_t virt)
{
	virt &= ~0xfff;
	uint64_t pte = (virt >> 12) & 511;
	volatile uint64_t* pt = (uint64_t*) (PG_PT | ((virt >>  9) & 0x0000007ffffff000ull) );

	pt[pte] = 0;

	asm volatile("invlpg (%0)" ::"r" (virt) : "memory");
}

inline void *operator new(size_t, void *p)     throw() { return p; }
inline void *operator new[](size_t, void *p)   throw() { return p; }
inline void  operator delete  (void *, void *) throw() { };
inline void  operator delete[](void *, void *) throw() { };

struct bitmap : public frame_alloc_iface
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

	uint64_t page() override
	{
		uint64_t p = find_zero();
		set(p);
		return p * 0x1000;
	}

	uint64_t mem_free()
	{
		uint64_t tot = 0;
		for (size_t i = 0; i < size; i++)
			if (!at(i))
				tot += 0x1000;

		return tot;
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

template<typename T>
class temp_page
{
public:
	temp_page(uint64_t virt, uint64_t phys, frame_alloc_iface* fai)
		: virt((T*) virt)
	{ map_page((uint64_t) virt, phys, fai); }

	~temp_page() { unmap_page((uint64_t) virt); }

	T operator [] (int idx) const { return virt[idx]; }
	T& operator [] (int idx) { return virt[idx]; }

private:
	T* virt;
};

void print_stack_use()
{
	putstring("stack usuage: ");
	put_hex_long(KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0));
	put_char('\n');
}

void clean_page_tables(bitmap* bm, frame_alloc_iface* fai)
{
	/* A place to temporarily map physical pages to */
	uint64_t tmp_virt = 0xffff8ffffffff000llu;

	volatile uint64_t* pml4 = (uint64_t*) PG_PML4;
	for (int i = 0; i < 0x200; i++) if (pml4[i] & 1)
	{
		temp_page<uint64_t> pdp(tmp_virt, pml4[i] & ~0xfff, fai);
		for (int j = 0; j < 0x200; j++) if (pdp[j] & 1)
		{
			temp_page<uint64_t> pd(tmp_virt + 0x1000, pdp[j] & ~0xfff, fai);
			for (int k = 0; k < 0x200; k++) if (pd[k] & 1)
			{
				temp_page<uint64_t> pt(tmp_virt + 0x2000, pd[k] & ~0xfff, fai);
				for (int l = 0; l < 0x200; l++) if (pt[l] & 1)
				{
					uint64_t virt = ((uint64_t) i) << 39 | ((uint64_t) j) << 30 |
						((uint64_t) k) << 21 | ((uint64_t) l) << 12;
					if (i <= 0x100)
					{
						/* Mark all pages references by the 'lower half'
						 * oh physical mem as available */
						bm->reset(pt[l] >> 12);
					}
					else
					{
						/* Mark the ones in the 'higher half' as used */
						virt |= 0xffff000000000000;
						if (!(virt >= tmp_virt && virt <= tmp_virt + 0x2000) )
							bm->set(pt[l] >> 12);
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

template<typename T, typename U>
static inline T mb_next(T entry, U tag)
{
	return (T) ((uint64_t) entry + (((U) tag)->entry_size));
}

void bitmap_setup(kernel_boot_info* kbi, struct multiboot_tag_mmap* mb_mmap_tag)
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

	putstring("Top of ram "); put_hex_long(top); put_char('\n');
	putstring("Total ram "); put_hex_long(total_ram); put_char('\n');

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

	/* Create our 'dumb' page frame allocator */
	new_page_dumb npd(kbi, (uint64_t) largest_region_ptr, (uint64_t) end_page);

	/* Lets allocate a bunch, we're just going to start allocating at the largest region and hope it's big enough */
	uint64_t allocced = 0;
	uint8_t* p = (uint8_t*) bitmapd;
	while (allocced < bitmap_size + sizeof(bitmap)) // Don't forget the size of the bitmap struct itself
	{
		auto pg = npd.page();
		if ((uint8_t*) pg >= end_page || !pg)
		{
			putstring("Ran out of memory for storing physical memory page bitmap\n");
			die();
		}

		map_page((uint64_t) p, (uint64_t) pg, &npd);
		p += 0x1000;
		allocced += 0x1000;
	}

	/* Construct the bitmap class at it's fixed address */
	auto bm = new(reinterpret_cast<void*>(bitmapd)) bitmap(top / 0x1000);

	/* Set all bits */
	bm->set_range(0, top / 0x1000);

	/* Iterate over the memory map again, and reset available bits. */
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb_next(entry, mb_mmap_tag))
	{
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			uint64_t start = (entry->addr + 0xfff) & ~0xfff;
			uint64_t end = (entry->addr + entry->len) & ~0xfff;

			if (end > start)
				bm->reset_range(start / 0x1000, end / 0x1000);
		}
	}

	/* This will mark the pages that are actually in use */	
	clean_page_tables(bm, &npd);

	uint64_t phys = 0xb8000;
	uint64_t virt = 0xffffffff40000000 - 0x1000;

	uint8_t* ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 126 * 1024 * 1024; i += 0x1000)
	{
		map_page((uint64_t) ppp, bm->page(), bm);
		ppp += 0x1000;
	}
	ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 10 * 1024 * 1024; i++)
	{
		ppp[i] = 0;
	}

	map_page(virt, phys, bm);

	volatile unsigned char* pp = (unsigned char*) virt;
	for (int i = 0; i < 4096; i++)
	{
		pp[i] = answer;
	}

	for(;;)asm("hlt");
	die();
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
			bitmap_setup(kbi, (struct multiboot_tag_mmap*) mb_tag);
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

/*
	uint64_t phys = 0xb8000;
	uint64_t virt = 0xffff900000000000;

	map_page(virt, phys);

	volatile unsigned char* p = (unsigned char*) virt;
	for (int i = 0; i < 4096; i++)
	{
		p[i] = answer;
	}
*/
	for (;;)
		asm("hlt");
}

extern "C" void _start(kernel_boot_info* _kbi)
{
	kbi = _kbi;
//	alloc_ptr =  0x1000000; // XXX: FIX THIS
	kmain();
}

