#include <cassert>

#include "mem.h"
#include "mb.h"
#include "new.h"
#include "frame_alloc.h"
#include "debug.h"
#include "config.h"
#include "linker.h"
#include "task.h"

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
	uint64_t bitmap_size = ((top + (PAGE_SIZE - 1)) / PAGE_SIZE + 7) / 8;
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
		p += PAGE_SIZE;
		allocced += PAGE_SIZE;
	}
	/* Construct the frame_alloc_bitmap class at it's fixed address */
	frame_alloc = new(reinterpret_cast<void*>(bitmapd)) frame_alloc_bitmap(top / PAGE_SIZE);

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
	volatile uint64_t* pml4 = (uint64_t*) (PG_PML4);
	for (uint64_t i = 0; i < 512; i++) if (pml4[i] & 1)
	{
		volatile uint64_t* pdp = (uint64_t*) (PG_PDP | (i << 12));
		for (uint64_t j = 0 ; j < 512; j++) if (pdp[j] & 1)
		{
			volatile uint64_t* pd = (uint64_t*) (PG_PD | ( (i << 21) | (j << 12) ) );
			for (uint64_t k = 0 ; k < 512; k++) if (pd[k] & 1)
			{
				volatile uint64_t* pt = (uint64_t*) (PG_PT | ( (i << 30) | (j << 21) | (k << 12) ) );
				for (uint64_t l = 0; l < 512; l++) if (pt[l] & 1)
					if (i > 0x100)
							get_bitmap()->set(pt[l] >> 12);
			}
		}
	}
}

void memory::setup_usage(multiboot_tag_mmap* mb_mmap_tag, uint64_t memtop)
{
	/* Set all bits */
	get_bitmap()->set_range(0, memtop / PAGE_SIZE);

	/* Iterate over the memory map again, and reset available bits. */
	multiboot_mmap_entry *entry;
	auto entry_end = (multiboot_mmap_entry*) ((uint64_t) mb_mmap_tag + mb_mmap_tag->size);
	for (entry = mb_mmap_tag->entries; entry < entry_end; entry = mb::next(entry, mb_mmap_tag))
	{
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			uint64_t start = (entry->addr + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
			uint64_t end = (entry->addr + entry->len) & ~(PAGE_SIZE - 1);

			if (end > start)
				get_bitmap()->reset_range(start / PAGE_SIZE, end / PAGE_SIZE);
		}
	}

	/* This will mark the pages that are actually in use */
	clean_page_tables();
}

void memory::map_page(uint64_t virt, uint64_t phys)
{
	assert((virt & (PAGE_SIZE - 1)) == 0);
	assert((phys & (PAGE_SIZE - 1)) == 0);

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
		pml4[pml4e] = (uint64_t) frame_alloc->page() | 0x107;
		clear_page((void*) pdp);
	}

	if (!(pdp[pdpe] & 1))
	{
		pdp[pdpe] = (uint64_t) frame_alloc->page() | 0x107;
		clear_page((void*) pd);
	}

	if (!(pd[pde] & 1))
	{
		pd[pde] = (uint64_t) frame_alloc->page() | 0x107;
		clear_page((void*) pt);
	}

	pt[pte] = phys | 0x107;

	invlpg(virt);
}

void memory::clear_page(void* page)
{
	uint64_t* p = (uint64_t*) page;

	for (int i = 0; i < 512; i++)
		p[i] = 0;
}

uint64_t memory::is_mapped(uint64_t virt)
{
	assert((virt & (PAGE_SIZE - 1)) == 0);
	uint64_t pml4e = (virt >> 39) & 511;
	uint64_t pdpe = (virt >> 30) & 511;
	uint64_t pde = (virt >> 21) & 511;
	uint64_t pte = (virt >> 12) & 511;

	volatile uint64_t* pml4 = (uint64_t*) (PG_PML4);
	volatile uint64_t* pdp = (uint64_t*) (PG_PDP | ((virt >> 27) & 0x00000000001ff000ull) );
	volatile uint64_t* pd = (uint64_t*) (PG_PD | ((virt >> 18) & 0x000000003ffff000ull) );
	volatile uint64_t* pt = (uint64_t*) (PG_PT | ((virt >>  9) & 0x0000007ffffff000ull) );

	if ((pml4[pml4e] & 1) && (pdp[pdpe] & 1) && (pd[pde] & 1) && (pt[pte] & 1))
		return pt[pte] & ~(PAGE_SIZE - 1);

	return 0;
}

void memory::unmap_page(uint64_t virt)
{
	assert((virt & (PAGE_SIZE - 1)) == 0);
	virt &= ~(PAGE_SIZE - 1);
	uint64_t pte = (virt >> 12) & 511;
	volatile uint64_t* pt = (uint64_t*) (PG_PT | ((virt >>  9) & 0x0000007ffffff000ull) );

	pt[pte] = 0;

	invlpg(virt);
}

uint64_t memory::get_phys(uint64_t virt)
{
	virt &= 0xffffffffffff;

	uint64_t* pt = (uint64_t*) PG_PT;
	uint64_t pte = virt >> 12;

	return (pt[pte] & ~(PAGE_SIZE - 1)) | (virt & (PAGE_SIZE - 1));
}

uint64_t memory::clone_tables()
{
	uint64_t tmp_virt = 0xffff8ffffffff000llu;
	uint64_t pml4_new = (uint64_t) frame_alloc->page();
	temp_page<uint64_t> tmp_pml4(tmp_virt, pml4_new, true);

	volatile uint64_t* pml4 = (uint64_t*) (PG_PML4);
	for (uint64_t i = 0; i < 512; i++) if (pml4[i] & 1)
	{
		if (i == PG_PML4E)
			continue;

		volatile uint64_t* pdp = (uint64_t*) (PG_PDP | (i << 12));
		for (uint64_t j = 0 ; j < 512; j++) if (pdp[j] & 1)
		{
			volatile uint64_t* pd = (uint64_t*) (PG_PD | ( (i << 21) | (j << 12) ) );
			for (uint64_t k = 0 ; k < 512; k++) if (pd[k] & 1)
			{
				volatile uint64_t* pt = (uint64_t*) (PG_PT | ( (i << 30) | (j << 21) | (k << 12) ) );
				for (uint64_t l = 0; l < 512; l++) if (pt[l] & 1)
				{
					if (!(tmp_pml4[i] & 1))
					{
						uint64_t new_pg = (uint64_t) frame_alloc->page();
						temp_page<uint64_t> tmp(tmp_virt + PAGE_SIZE * 1, new_pg, true);
						tmp_pml4[i] = new_pg | (pml4[i] & (PAGE_SIZE - 1));
					}

					temp_page<uint64_t> tmp_pdp(tmp_virt + PAGE_SIZE * 1, tmp_pml4[i] & ~(PAGE_SIZE - 1));
					if (!(tmp_pdp[j] & 1))
					{
						uint64_t new_pg = (uint64_t) frame_alloc->page();
						temp_page<uint64_t> tmp(tmp_virt + PAGE_SIZE * 2, new_pg, true);
						tmp_pdp[j] = new_pg | (pdp[j] & (PAGE_SIZE - 1));
					}

					temp_page<uint64_t> tmp_pd(tmp_virt + PAGE_SIZE * 2, tmp_pdp[j] & ~(PAGE_SIZE - 1));
					if (!(tmp_pd[k] & 1))
					{
						uint64_t new_pg = (uint64_t) frame_alloc->page();
						temp_page<uint64_t> tmp(tmp_virt + PAGE_SIZE * 3, new_pg, true);
						tmp_pd[k] = new_pg | (pd[k] & (PAGE_SIZE - 1));
					}

					temp_page<uint64_t> tmp_pt(tmp_virt + PAGE_SIZE * 3, tmp_pd[k] & ~(PAGE_SIZE - 1));
					tmp_pt[l] = pt[l];
				}
			}
		}
	}

	/* Make the new page-table self-referencing */
	tmp_pml4[PG_PML4E] = (uint32_t) pml4_new | 0x3;

	return pml4_new;
}

extern std::shared_ptr<task> current;

bool memory::handle_pg_fault(interrupt_state*, uint64_t addr)
{
	if (!current)
		return false;

	for (auto& mbi: current->mapped_io_handles)
	{
		if (addr >= mbi.addr && addr < mbi.addr + mbi.len)
		{   
			/* Align page */
			addr &= ~(PAGE_SIZE - 1);

			/* Map the page */
			memory::map_page(addr, memory::frame_alloc->page());

			/* Seek and read the page */
			mbi.handle->seek(mbi.file_offs + addr - mbi.addr);
			ssize_t r = mbi.handle->read((void*) addr, PAGE_SIZE);

			if (r == -1)
				con << "read() in memory::handle_pg_fault returned -1\n";
			else
				if (r < PAGE_SIZE) /* Pad ? */
					for (auto i = r; i < PAGE_SIZE; i++)
						*((uint8_t*) addr + i) = 0;

			return true;
		}
	}

	return false;
}

