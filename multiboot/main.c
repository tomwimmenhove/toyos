#include <stdint.h>
#include "multiboot2.h"

#include "cpuid.h"
#include "sysregs.h"
#include "gdt.h"
#include "elf.h"
#include "jump.h"
#include "../common/debug_out.h"

extern void* _stack_top;
extern void* _data_end;

static uint64_t kernel_stack_top = 0xfffffffff0000000llu;
static uint64_t alloc_ptr = 0x00008000; /* guaranteed free for use according to https://wiki.osdev.org/Memory_Map_(x86) */
static uint64_t* pml4;

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

int has_long_mode()
{
	uint32_t a, x;

	/* Get Highest Extended Function Supported. Should be at least 0x80000001 (next cpuid call). */
	cpuid(0x80000000, 0, &a, &x, &x, &x);
	putstring("CPUID: Highest Extended Function Supported: ");
	put_hex_int(a);
	put_char('\n');
	if (a < 0x80000001)
		return 0;

	/* Get Extended Processor Info and Feature Bits and check for the Long Mode bit (29) */
	cpuid(0x80000001, 0, &x, &x, &x, &a);
	return a & (1 << 29);
}

uint64_t new_page()
{
	uint64_t p = alloc_ptr;
	alloc_ptr += 0x1000;
	return p;
}

void clear_page(void* addr)
{
	uint64_t* p = (uint64_t*) addr;
	for (int i = 0; i < 512; i++)
	{
		p[i] = 0;
	}
}

uint32_t new_clean_page()
{
	uint32_t p = new_page();
	clear_page((void*) p);
	return p;
}

void map_page(uint64_t virt, uint64_t phys)
{
#if 0
	putstring("Mapping address ");
	put_hex_long(virt);
	putstring(" to ");
	put_hex_long(phys);
	put_char('\n');
#endif

	if (virt & 0xfff || phys & 0xfff)
	{
		putstring("Trying to map on a non-page boundary.\n");
		die();
	}

	uint16_t pt_idx = (virt >> 12) & 0x1ff;
	uint16_t pd_idx = (virt >> 21) & 0x1ff;
	uint16_t pdp_idx = (virt >> 30) & 0x1ff;
	uint16_t pml4_idx = (virt >> 39) & 0x1ff;

	/* Check for level 4 entry in PML4 table */
	if (!(pml4[pml4_idx] & 1))
		pml4[pml4_idx] = (uint32_t) new_clean_page() | 3;

	/* Check for level 3 entry in PDP table */
	uint64_t* pdp = (uint64_t*) (uint32_t) (pml4[pml4_idx] & ~0xfff);
	if (!(pdp[pdp_idx] & 1))
		pdp[pdp_idx] = (uint32_t) new_clean_page() | 3;

	/* Check for level 2 entry in PD table */
	uint64_t* pd = (uint64_t*) (uint32_t) (pdp[pdp_idx] & ~0xfff);
	if (!(pd[pd_idx] & 1))
		pd[pd_idx] = (uint32_t) new_clean_page() | 3;

	/* Get the level 1 entry in the PT table */
	uint64_t* pt = (uint64_t*) (uint32_t) (pd[pd_idx] & ~0xfff);

	/* Set the entry in the page table */
	pt[pt_idx] = phys | 3;
}

void map_pages(uint64_t virt, uint64_t phys, uint64_t size)
{
	for (uint64_t i = 0; i < size; i += 0x1000, virt += 0x1000, phys += 0x1000)
	{
		map_page(virt, phys);
	}
}

void alloc_pages(uint64_t virt, uint64_t size)
{
	for (uint64_t i = 0; i < size; i += 0x1000, virt += 0x1000)
	{
		map_page(virt++, (uint32_t) new_page());
	}
}

void setup_page_tables()
{
	putstring("Setting up page tables: Identity map lower 2MB\n");
	pml4 = (uint64_t*) (uint32_t) new_clean_page();
	cr3_set((uint32_t) pml4);

	/* Identity-map every page from 1MB to _data_end */
	map_pages(0x100000, 0x100000, ((((uint32_t) &_data_end) + 0xfff) - 0x1000) & ~0xfff);

	/* Map one page for the kernel stack */
	map_pages(kernel_stack_top, (uint64_t) (uint32_t) new_page(), 0x1000);

	/* Make page-tables self-referencing */
	pml4[511] = (uint32_t) pml4 | 0x3;

	return;
}

uint64_t load_kernel(struct multiboot_tag_module *module)
{
	uint64_t entry = 0;
	uint8_t* module_ptr = (uint8_t*) module->mod_start;

	putstring("Module start at ");
	put_hex_int(module->mod_start);
	put_char('\n');

	Elf64_Ehdr* hdr = (Elf64_Ehdr*) module->mod_start;

	if (hdr->e_ident[0] != 0x7f || hdr->e_ident[1] != 'E' || hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F')
	{
		putstring("No ELF header found in kernel\n");
		die();
	}

	entry = hdr->e_entry;
	putstring("Entry point at ");
	put_hex_long(hdr->e_entry);
	put_char('\n');

	uint8_t* space = (uint8_t*) &_data_end;

	for (int i = 0; i < hdr->e_phnum; i++)
	{
		Elf64_Phdr* phdr = (Elf64_Phdr*) &module_ptr[hdr->e_phoff + i * hdr->e_phentsize];

		putstring("Segment at ");
		put_hex_long(phdr->p_vaddr);
		putstring(" of size ");
		put_hex_long(phdr->p_memsz);
		putstring(": ");

		switch(phdr->p_type)
		{
			case 0:
				putstring("Unused entry\n");
				break;
			case 1:
			{
				putstring("Loadable segment\n");

				uint8_t* ptr = (uint8_t*) (uint32_t) phdr->p_vaddr;
				if ((uint32_t) ptr & 0xfff)
				{
					putstring("Segment is not page-aligned!\n");
					die();
				}
				if (!phdr->p_vaddr)
					break;

				/* Page-align */
				space = (uint8_t*) ((((uint32_t) space) + 0xfff) & ~0xfff);

				/* Map the kernel code/data into virtual address space (for when paging becomes enabled. */
				map_pages(phdr->p_vaddr, (uint32_t) space, (phdr->p_memsz + 4095) & ~0xfff);

				/* Copy the segment */
				for (uint64_t i = 0; i < phdr->p_filesz; i++)
					*space++ = module_ptr[phdr->p_offset + i];
				/* Padding */
				for (uint64_t i = phdr->p_filesz; i < phdr->p_memsz; i++)
					*space++ = 0;

				break;
			}
			case 2:
				putstring("Dynamic linking tables\n");
				break;
			case 3:
				putstring("Program interpreter path name\n");
				break;
			case 4:
				putstring("Note sections\n");
				break;
			default:
				putstring("Section type ");
				put_hex_int(phdr->p_type);
				put_char('\n');
				break;
		}
	}

	return entry;
}

void c_entry(unsigned int magic, unsigned long addr)
{
	if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
	{
		putstring("Bad magic number: ");
		put_hex_int(magic); put_char('\n');
		die();
	}

	if (!has_cpuid())
	{
		putstring("CPU doesn't support CPUID.\n");
		die();
	}

	if (!has_long_mode())
	{
		putstring("CPU doesn't support long mode.\n");
		die();
	}

	setup_page_tables();

	addr += 8;
	uint64_t entry = 0;
	struct multiboot_tag* mb_tag = (struct multiboot_tag *) addr;
	while (mb_tag->type)
	{
		switch (mb_tag->type)
		{
			case MULTIBOOT_TAG_TYPE_MODULE:
			{
				struct multiboot_tag_module *mb_mod_tag = (struct multiboot_tag_module*) mb_tag;
				(void) mb_mod_tag;
				putstring("Module tag: Cmdline=");
				putstring(mb_mod_tag->cmdline);
				putstring(",Size=");
				put_hex_int(mb_mod_tag->mod_end - mb_mod_tag->mod_start);
				put_char('\n');

				if (entry)
				{
					putstring("Second kernel module found! There can only be one.\n");
					die();
				}

				entry = load_kernel(mb_mod_tag);

				break;
			}
			case MULTIBOOT_TAG_TYPE_MMAP:
			{
				struct multiboot_tag_mmap *mp_mmap_tag = (struct multiboot_tag_mmap*) mb_tag;

				for (struct multiboot_mmap_entry *entry = mp_mmap_tag->entries;
						(uint8_t *) entry < (uint8_t *) mb_tag + mb_tag->size;
						entry = (multiboot_memory_map_t *) ((unsigned long) entry
							+ ((struct multiboot_tag_mmap *) mb_tag)->entry_size))
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
				break;
			}
			default:
				break;
		}

		addr += (mb_tag->size + 7) & ~7;
		mb_tag = (struct multiboot_tag *) addr;
	}

	if (!entry)
	{
		putstring("No kernel module!\n");
		die();
	}

	/* Setting Physical Address Extension bit (5) in control register 4 */
	putstring("Enabling Physical Address Extension\n");
	uint32_t cr4 = cr4_get();
	cr4_set(cr4 | 1 << 5);

	/* Setting Long Mode Enable bit (8) in Extended Feature Enable Register (0xC0000080) in MSR*/
	putstring("Enable long mode in EFER\n");
	uint32_t efer = rdmsr(0xC0000080);
	wrmsr(0xC0000080, efer | 1 << 8);

	/* Setting Paging bit (31) in control register 0 */
	putstring("Enable paging\n");
	uint32_t cr0 = cr0_get();
	cr0_set(cr0 | 1 << 31);

	/* Setup the GDT */
	putstring("Setting up the Global Descriptor Table\n");
	uint64_t gdt[] =
	{
		/* Null descriptor */
		0,

		/* Code Segment Descriptor */
		mk_desc(	0xfffff,	/* Segment limit */
				0,		/* Base address */
				8,		/* Type: Execute-Only Code-Segment */
				1,		/* S-field: User */
				0,		/* Privilege: */
				1,		/* Present */

				1,		/* Long mode */
				1,		/* Default operand size: 64 bit */

				1),		/* Granularity: Scale limit by 4096 */

		/* Data Segment Descriptor */
		mk_desc(	0xfffff,	/* Segment limit */
				0,		/* Base address */
				2,		/* Type: Read/Write Data-Segment */
				1,		/* S-field: User */
				0,		/* Privilege: */
				1,		/* Present */
				0,		/* Unused */
				1,		/* Default operand size: 32 bit */
				1),		/* Granularity: Scale limit by 4096 */
	};

	struct gdt_ptr gdtp = 
	{
		sizeof(gdt) - 1,
		gdt
	};
	lgdt(&gdtp);

	putstring("Jumping to Long mode kernel code\n");
	jump_kernel(kernel_stack_top, entry);

	die();
}

