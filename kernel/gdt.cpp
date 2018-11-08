#include <stddef.h>

#include "gdt.h"

/* Setup our global descriptor table */
static desc_null kernel_null_descriptor;
static desc_code_seg kernel_code_segment_descriptor { false, 0, true, true, false };
static desc_data_seg kernel_data_segment_descriptor { 0, true, 0, true };
static desc_code_seg interrupt_code_segment_descriptor { false, 0, true, true, false };

static desc_code_seg user_code_segment_descriptor { false, 3, true, true, false };
static desc_data_seg user_data_segment_descriptor { 0, true, 3, true };

tss tss0 { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static desc_tss desc_tss0 { &tss0, 3, true };

static uint8_t gdt[gdt_max_size];

static desc_ptr gdt_ptr { (gdt_entry*) gdt, sizeof(gdt) };

template<typename T>
void gdt_load_entry(uint16_t offset, T entry)
{
	void* p = &gdt[offset]; // Makes GCC happy
	*((T*) p) = entry;
}

void gdt_init()
{
	for (size_t i = 0; i < sizeof(gdt); i += sizeof(desc_null))
		gdt_load_entry(0x00, kernel_null_descriptor);
	gdt_load_entry(0x08, kernel_code_segment_descriptor);
	gdt_load_entry(0x10, kernel_data_segment_descriptor);
	gdt_load_entry(0x18, interrupt_code_segment_descriptor);

	gdt_load_entry(0x20, desc_tss0);

	gdt_load_entry(0x30, user_code_segment_descriptor);
	gdt_load_entry(0x38, user_data_segment_descriptor);

	/* Load the GDT register */
	gdt_ptr.lgdt();

	/* Load the code and data segment */
	asm volatile(   "sub $16, %%rsp\n"              // Make space
					"movq $0x8, 8(%%rsp)\n"         // Set code segment
					"movabsq $jmp_lbl, %%rax\n"     // Set return address (label)...
					"mov %%rax, (%%rsp)\n"          // ... at stack pointer
					"lretq\n"                       // Jump
					"jmp_lbl:\n"

					"mov $0x10, %%ecx\n"            // Load the data segment
					"mov %%ecx, %%ds\n"             // Stick it in all data-seg regs
					"mov %%ecx, %%es\n"
					"mov %%ecx, %%fs\n"
					"mov %%ecx, %%gs\n"
					"mov %%ecx, %%ss\n"
					: : : "%ecx");
}

