#include "memory.h"
#include "mb.h"
#include "new.h"
#include "frame_alloc.h"
#include "debug.h"
#include "config.h"
#include "linker.h"

frame_alloc_iface* memory::frame_alloc;

void memory::init(kernel_boot_info* kbi)
{
	auto mb_mmap_tag = mb::get_tag<multiboot_tag_mmap*>(kbi, MULTIBOOT_TAG_TYPE_MMAP);
	if (!mb_mmap_tag)
	{
		panic("No multiboot memory map found");
		die();
	}

	multiboot_mmap_entry *entry;
	auto entry_end = (multiboot_mmap_entry*) ((uint64_t) mb_mmap_tag + mb_mmap_tag->size);

	uint64_t top = 0;
	uint8_t* largest_region_ptr = nullptr;
	uint64_t largest_region_len = 0;

	uint64_t total_ram = 0;

	/* Count RAM */
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb::next(entry, mb_mmap_tag))
	{
		const char *typeNames[] = { "Unknown", "available", "reserved", "ACPI reclaimable", "NVS", "\"bad ram\""};
		con << "Memory region " <<  entry->addr << " with length: " << entry->len << " of type " << typeNames[entry->type] << '\n';

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
	con << "Top of ram " << top <<'\n';
	con << "Total ram " << total_ram << '\n';

	if (!largest_region_len)
		panic("No space found for physical memory page frame_alloc_bitmap");

	auto end_page = largest_region_ptr + largest_region_len;

	/* We need one bit per page */
	uint64_t bitmap_size = ((top + 0xfff) / 0x1000 + 7) / 8;
	uint8_t* bitmapd = (uint8_t*) 0xffffffff40000000;

	/* Create our temporary 'dumb' page frame allocator */
	frame_alloc_dumb fad(kbi, (uint64_t) largest_region_ptr, (uint64_t) end_page);
	frame_alloc = &fad;

	/* Lets allocate the memory for the frame_alloc_bitmap, we're just going to start allocating at the largest region and hope it's big enough */
	uint64_t allocced = 0;
	uint8_t* p = (uint8_t*) bitmapd;
	while (allocced < bitmap_size + sizeof(frame_alloc_bitmap)) // Don't forget the size of the frame_alloc_bitmap struct itself
	{
		auto pg = fad.page();
		if ((uint8_t*) pg >= end_page || !pg)
		{
			panic("Ran out of memory for storing physical memory page frame_alloc_bitmap");
		}

		map_page((uint64_t) p, (uint64_t) pg);
		p += 0x1000;
		allocced += 0x1000;
	}
	/* Construct the frame_alloc_bitmap class at it's fixed address */
	frame_alloc = new(reinterpret_cast<void*>(bitmapd)) frame_alloc_bitmap(top / 0x1000);

	/* Setup the frame_alloc_bitmap and unmap unused pages */
	setup_usage(mb_mmap_tag, top);
}

void memory::unmap_unused()
{
	volatile uint64_t* pml4 = (uint64_t*) PG_PML4;

	/* We can simply unmap everything in the lower half */
	for (int i = 0; i < 0x100; i++)
		pml4[i] = 0;

	/* Burtally invalidate all TLB cache */
	cr3_set(cr3_get());
}

void memory::clean_page_tables()
{
	/* A place to temporarily map physical pages to */
	uint64_t tmp_virt = 0xffff8ffffffff000llu;

	volatile uint64_t* pml4 = (uint64_t*) PG_PML4;
	for (int i = 0; i < 0x200; i++) if (pml4[i] & 1)
	{
		temp_page<uint64_t> pdp(tmp_virt, pml4[i] & ~0xfff);
		for (int j = 0; j < 0x200; j++) if (pdp[j] & 1)
		{
			temp_page<uint64_t> pd(tmp_virt + 0x1000, pdp[j] & ~0xfff);
			for (int k = 0; k < 0x200; k++) if (pd[k] & 1)
			{
				temp_page<uint64_t> pt(tmp_virt + 0x2000, pd[k] & ~0xfff);
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
}

void memory::setup_usage(multiboot_tag_mmap* mb_mmap_tag, uint64_t memtop)
{
	/* Set all bits */
	get_bitmap()->set_range(0, memtop / 0x1000);

	/* Iterate over the memory map again, and reset available bits. */
	multiboot_mmap_entry *entry;
	auto entry_end = (multiboot_mmap_entry*) ((uint64_t) mb_mmap_tag + mb_mmap_tag->size);
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb::next(entry, mb_mmap_tag))
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
		pml4[pml4e] = (uint64_t) frame_alloc->page() | 7;
		clear_page((void*) pdp);
	}

	if (!(pdp[pdpe] & 1))
	{
		pdp[pdpe] = (uint64_t) frame_alloc->page() | 7;
		clear_page((void*) pd);
	}

	if (!(pd[pde] & 1))
	{
		pd[pde] = (uint64_t) frame_alloc->page() | 7;
		clear_page((void*) pt);
	}

	pt[pte] = phys | 7;
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

void memory::handle_pg_fault(exception_state*, uint64_t)
{
}

