#include <cstdint>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/multiboot2.h"
	#include "../common/config.h"
}

extern void* _data_end;
uint8_t* last_page;

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

void clean_page_tables()
{
	uint64_t* pml4 = (uint64_t*) 0xffffff7fbfdfe000llu;
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

void parse_memmap(kernel_boot_info* kbi, struct multiboot_tag_mmap* mb_mmap_tag)
{
	for (struct multiboot_mmap_entry *entry = mb_mmap_tag->entries;
			(uint8_t *) entry < (uint8_t *) mb_mmap_tag + mb_mmap_tag->size;
			entry = (multiboot_memory_map_t *) ((unsigned long) entry
				+ ((struct multiboot_tag_mmap *) mb_mmap_tag)->entry_size))
	{
		putstring("Memory region ");
		put_hex_long(entry->addr);
		putstring(" with length: ");
		put_hex_long(entry->len);
		putstring(" of type ");
		const char *typeNames[] = { "Unknown", "available", "reserved", "ACPI reclaimable", "NVS", "\"bad ram\""};

		putstring(typeNames[entry->type]);
		put_char('\n');

	}

	(void)kbi;
}

void parse_kbi(kernel_boot_info* kbi)
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

int kmain(kernel_boot_info* kbi)
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

	parse_kbi(kbi);

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

extern "C" void _start(kernel_boot_info* kbi)
{
	last_page = (uint8_t*) 0x1000000; // XXX: FIX THIS
	kmain(kbi);
}

