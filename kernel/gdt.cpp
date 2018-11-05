#include "descriptors.h"

/* Setup our global descriptor table */
static desc_null kernel_null_descriptor;
static desc_code_seg kernel_code_segment_descriptor { false, 0, true, true, false };
static desc_data_seg kernel_data_segment_descriptor { 0, true, 0, true };
static desc_code_seg interrupt_code_segment_descriptor { false, 0, true, true, false };

static gdt_entry descs[] = {
	kernel_null_descriptor,             // 0x00
	kernel_code_segment_descriptor,     // 0x08
	kernel_data_segment_descriptor,     // 0x10
	interrupt_code_segment_descriptor,  // 0x18
};

static desc_ptr gdt_ptr { descs, sizeof(descs) };

void gdt_init()
{
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

