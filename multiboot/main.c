#include <stdint.h>

#include "cpuid.h"
#include "sysregs.h"
#include "gdt.h"
#include "elf.h"
#include "jump.h"
#include "../common/multiboot2.h"
#include "../common/debug_out.h"
#include "../common/config.h"

extern void* _stack_top;
extern void* _data_end;

static uint64_t alloc_ptr = ALLOC_START;
static uint64_t* pml4;

/* Relocate by copying or map module into virtual address space directly */
#define RELOCATE

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
		p[i] = 0;
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

	uint16_t pte = (virt >> 12) & 0x1ff;
	uint16_t pde = (virt >> 21) & 0x1ff;
	uint16_t pdpe = (virt >> 30) & 0x1ff;
	uint16_t pml4e = (virt >> 39) & 0x1ff;

	/* Check for level 4 entry in PML4 table */
	if (!(pml4[pml4e] & 1))
		pml4[pml4e] = (uint32_t) new_clean_page() | 7;

	/* Check for level 3 entry in PDP table */
	uint64_t* pdp = (uint64_t*) (uint32_t) (pml4[pml4e] & ~0xfff);
	if (!(pdp[pdpe] & 1))
		pdp[pdpe] = (uint32_t) new_clean_page() | 7;

	/* Check for level 2 entry in PD table */
	uint64_t* pd = (uint64_t*) (uint32_t) (pdp[pdpe] & ~0xfff);
	if (!(pd[pde] & 1))
		pd[pde] = (uint32_t) new_clean_page() | 7;

	/* Get the level 1 entry in the PT table */
	uint64_t* pt = (uint64_t*) (uint32_t) (pd[pde] & ~0xfff);

	/* Set the entry in the page table */
	pt[pte] = phys | 7;
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

void setup_page_tables()
{
	putstring("Setting up page tables: Identity map lower 2MB\n");
	pml4 = (uint64_t*) (uint32_t) new_clean_page();
	cr3_set((uint32_t) pml4);

	/* Identity-map every page from ALLOC_START to _data_end */
	map_pages(ALLOC_START, ALLOC_START, ((uint32_t) &_data_end) - ALLOC_START);

	/* Map one page for the kernel stack */
	map_pages(KERNEL_STACK_TOP - 0x1000, (uint64_t) (uint32_t) new_page(), 0x1000);

	/* Make page-tables self-referencing */
	pml4[PG_PML4E] = (uint32_t) pml4 | 0x3;

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

#ifdef RELOCATE
	uint8_t* space = (uint8_t*) KERNEL_ALLOC_START;
#endif

	for (int i = 0; i < hdr->e_phnum; i++)
	{
		Elf64_Phdr* phdr = (Elf64_Phdr*) &module_ptr[hdr->e_phoff + i * hdr->e_phentsize];

		putstring("Segment at ");
		put_hex_long(phdr->p_vaddr);
		putstring(" of size ");
		put_hex_long(phdr->p_memsz);
		putstring(" at file offset ");
		put_hex_int((uint32_t) phdr->p_offset);
		putstring(": ");

		switch(phdr->p_type)
		{
			case 0:
				putstring("Unused entry\n");
				break;
			case 1:
			{
				putstring("Loadable segment\n");

				if (phdr->p_vaddr & 0xfff)
				{
					putstring("Segment is not page-aligned!\n");
					die();
				}
				if (!phdr->p_vaddr)
					break;
#ifdef RELOCATE
				putstring("Relocating...\n");

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
#else
				if ((uint32_t) &module_ptr[phdr->p_offset] & 0xfff)
				{
					putstring("File-offset is not page-aligned!\n");
					die();
				}

				/* Map directly into place. From the looks of the kernel images, we don't even need to pad? */
				map_pages(phdr->p_vaddr, (uint64_t) (uint32_t) &module_ptr[phdr->p_offset],
						(phdr->p_memsz + 4095) & ~0xfff);
				// XXX: Not sure how to deal with .bss etc... Using relocation for now.
#endif

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

void c_entry(unsigned int magic, uint32_t mb_addr)
{
	if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
	{
		putstring("Bad magic number: ");
		put_hex_int(magic); put_char('\n');
		die();
	}

	putstring("Multiboot structure at ");
	put_hex_int(mb_addr);
	put_char('\n');

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

	mb_addr += 8;
	uint32_t addr = mb_addr;
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
				0,		/* Default operand size: 32 bit */

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
	kernel_boot_info kbi = { KBI_MAGIC, ALLOC_START, (uint64_t) (uint32_t) &_data_end, mb_addr };
	jump_kernel(entry, KERNEL_STACK_TOP, (uint64_t) (uint32_t) &kbi);

	die();
}

