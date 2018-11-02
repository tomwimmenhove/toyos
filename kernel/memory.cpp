#include "memory.h"
#include "mb.h"
#include "new.h"

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/config.h"
}

extern void* _data_end;
extern void* _code_start;


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
					(p >= memory::get_phys((uint64_t) &_code_start) && p <= memory::get_phys((uint64_t) &_data_end - 0x1000)))
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


memory::memory(kernel_boot_info* kbi)
{
	auto mb_mmap_tag = get_mem_map(kbi);
	if (!mb_mmap_tag)
	{
		putstring("No multiboot memory map found\n");
		die();
	}

	struct multiboot_mmap_entry *entry;
	auto entry_end = (struct multiboot_mmap_entry*) ((uint64_t) mb_mmap_tag + mb_mmap_tag->size);

	uint64_t top = 0;
	uint8_t* largest_region_ptr = nullptr;
	uint64_t largest_region_len = 0;

	uint64_t total_ram = 0;

	/* Count RAM */
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

	auto end_page = largest_region_ptr + largest_region_len;

	/* We need one bit per page */
	uint64_t bitmap_size = ((top + 0xfff) / 0x1000 + 7) / 8;
	uint8_t* bitmapd = (uint8_t*) 0xffffffff40000000;

	/* Create our temporary 'dumb' page frame allocator */
	new_page_dumb npd(kbi, (uint64_t) largest_region_ptr, (uint64_t) end_page);
	frame_alloc = &npd;

	/* Lets allocate the memory for the bitmap, we're just going to start allocating at the largest region and hope it's big enough */
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

		map_page((uint64_t) p, (uint64_t) pg);
		p += 0x1000;
		allocced += 0x1000;
	}
	/* Construct the bitmap class at it's fixed address */
	frame_alloc = new(reinterpret_cast<void*>(bitmapd)) bitmap(top / 0x1000);

	/* Setup the bitmap and unmap unused pages */
	setup_usage(mb_mmap_tag, top);

	// XXX: Just a test!
	uint8_t* ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 1 * 1024 * 1024; i += 0x1000)
	{
		map_page((uint64_t) ppp, get_bitmap()->page());
		ppp += 0x1000;
	}
	ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 1 * 1024 * 1024; i++)
	{
		ppp[i] = 65;
	}
}

void memory::clean_page_tables()
{
	/* A place to temporarily map physical pages to */
	uint64_t tmp_virt = 0xffff8ffffffff000llu;

	volatile uint64_t* pml4 = (uint64_t*) PG_PML4;
	for (int i = 0; i < 0x200; i++) if (pml4[i] & 1)
	{
		temp_page<uint64_t> pdp(tmp_virt, pml4[i] & ~0xfff, this);
		for (int j = 0; j < 0x200; j++) if (pdp[j] & 1)
		{
			temp_page<uint64_t> pd(tmp_virt + 0x1000, pdp[j] & ~0xfff, this);
			for (int k = 0; k < 0x200; k++) if (pd[k] & 1)
			{
				temp_page<uint64_t> pt(tmp_virt + 0x2000, pd[k] & ~0xfff, this);
				for (int l = 0; l < 0x200; l++) if (pt[l] & 1)
				{
					uint64_t virt = ((uint64_t) i) << 39 | ((uint64_t) j) << 30 |
						((uint64_t) k) << 21 | ((uint64_t) l) << 12;
					if (i > 0x100)
					{
						/* Mark the ones in the 'higher half' as used */
						virt |= 0xffff000000000000;
						if (!(virt >= tmp_virt && virt <= tmp_virt + 0x2000) )
							get_bitmap()->set(pt[l] >> 12);
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

void memory::setup_usage(multiboot_tag_mmap* mb_mmap_tag, uint64_t memtop)
{
	/* Set all bits */
	get_bitmap()->set_range(0, memtop / 0x1000);

	/* Iterate over the memory map again, and reset available bits. */
	struct multiboot_mmap_entry *entry;
	auto entry_end = (struct multiboot_mmap_entry*) ((uint64_t) mb_mmap_tag + mb_mmap_tag->size);
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb_next(entry, mb_mmap_tag))
	{
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			uint64_t start = (entry->addr + 0xfff) & ~0xfff;
			uint64_t end = (entry->addr + entry->len) & ~0xfff;

			if (end > start)
				get_bitmap()->reset_range(start / 0x1000, end / 0x1000);
		}
	}

	/* This will mark the pages that are actually in use */
	clean_page_tables();
}

multiboot_tag_mmap* memory::get_mem_map(kernel_boot_info* kbi)
{
	auto mb_tag_addr = kbi->mb_tag;
	auto mb_tag = (struct multiboot_tag *) mb_tag_addr;
	while (mb_tag->type)
	{
		mb_tag_addr += (mb_tag->size + 7) & ~7;
		mb_tag = (struct multiboot_tag *) mb_tag_addr;

		if (mb_tag->type == MULTIBOOT_TAG_TYPE_MMAP)
		{
			return (multiboot_tag_mmap*) mb_tag;
		}
	}

	return nullptr;
}

void memory::map_page(uint64_t virt, uint64_t phys)
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
		pml4[pml4e] = (uint64_t) frame_alloc->page() | 3;
		clear_page((void*) pdp);
	}

	if (!(pdp[pdpe] & 1))
	{
		pdp[pdpe] = (uint64_t) frame_alloc->page() | 3;
		clear_page((void*) pd);
	}

	if (!(pd[pde] & 1))
	{
		pd[pde] = (uint64_t) frame_alloc->page() | 3;
		clear_page((void*) pt);
	}

	pt[pte] = phys | 3;
}

void memory::clear_page(void* page)
{
	uint64_t* p = (uint64_t*) page;

	for (int i = 0; i < 512; i++)
	{
		p[i] = 0;
	}
}

void memory::unmap_page(uint64_t virt)
{
	virt &= ~0xfff;
	uint64_t pte = (virt >> 12) & 511;
	volatile uint64_t* pt = (uint64_t*) (PG_PT | ((virt >>  9) & 0x0000007ffffff000ull) );

	pt[pte] = 0;

	asm volatile("invlpg (%0)" ::"r" (virt) : "memory");
}

uint64_t memory::get_phys(uint64_t virt)
{
	virt &= 0xffffffffffff;

	uint64_t* pt = (uint64_t*) PG_PT;
	uint64_t pte = virt >> 12;

	return (pt[pte] & ~0xfff) | (virt & 0xfff);
}

