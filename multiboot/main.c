#include <stdint.h>
#include "multiboot2.h"

#include "cpuid.h"
#include "sysregs.h"
#include "gdt.h"
#include "elf.h"
#include "debug_out.h"

extern void* _data_end2;

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

extern void* _data_end;
uint8_t* last_page = (uint8_t*) 0x00000000;

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

void* new_clean_page()
{
	void* p = new_page();
	clear_page(p);
	return p;
}

static uint64_t* pml4;

void map_page(uint64_t virt, uint64_t phys)
{
	uint16_t pt_idx = virt & 0x1ff;
	uint16_t pd_idx = (virt >> 9) & 0x1ff;
	uint16_t pdp_idx = (virt >> 18) & 0x1ff;
	uint16_t pml4_idx = (virt >> 27) & 0x1ff;

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
	pt[pt_idx] = (phys << 12) | 3;
}

void map_pages(uint64_t virt, uint64_t phys, uint64_t pages)
{
	while (pages--)
		map_page(virt++, phys++);
}

void alloc_pages(uint64_t virt, uint64_t pages)
{
	while (pages--)
		map_page(virt++, (uint32_t) new_page());
}

void setup_page_tables()
{
	putstring("Setting up page tables: Identity map lower 2MB\n");
	pml4 = (uint64_t*) new_page();
	cr3_set((uint32_t) pml4);

	clear_page(pml4);

	/* Identity-map every page from page 1 to _data_end */
	map_pages(1, 1, ((((uint32_t) &_data_end) + 0xfff) >> 12) - 1);

	/* Make page-tables self-referencing */
	pml4[511] = (uint32_t) pml4 | 0x3;

	return;
}

uint32_t load_kernel(struct multiboot_tag_module *module)
{
	uint32_t entry = 0;
	uint8_t* module_ptr = (uint8_t*) module->mod_start;

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
				if (!ptr)
					break;

				/* Page-align */
				space = (uint8_t*) ((((uint32_t) space) + 0xfff) & ~0xfff);

				/* Map the kernel code/data into virtual address space (for when paging becomes enabled. */
				map_pages(phdr->p_vaddr >> 12, ((uint32_t) space) >> 12, (phdr->p_memsz + 4095) >> 12);

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

extern void* _stack_top;

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
	uint32_t entry = 0;
	struct multiboot_tag* mb_tag = (struct multiboot_tag *) addr;
	while (mb_tag->type)
	{
		switch (mb_tag->type)
		{
			case MULTIBOOT_TAG_TYPE_MODULE:
			{
				struct multiboot_tag_module *mb_mod_tag = (struct multiboot_tag_module *) mb_tag;
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
	uint64_t test_gdt[] =
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
		sizeof(test_gdt) - 1,
		test_gdt
	};
	lgdt(&gdtp);

	putstring("Jumping to Long mode kernel code\n");
	set_data_segs(0x0010);
	far_jmp(0x0008, entry);

	die();
}

