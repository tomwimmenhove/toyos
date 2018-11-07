#include "descriptors.h"

/* Setup our interrupt descriptor table */
static idt_entry idt[256];
static desc_ptr idt_ptr { idt, sizeof(idt) };

/* Pointer to the first interrupt routing. They are space 0x10 bytes apart */
extern "C" void intr_0();

void idt_init()
{
	/* Fill all entries */
	for (int i = 0; i < 256; i++)
		// XXX: Hard-coded syscall entry. Ugly fucker!
		idt[i] = idt_entry(((uint64_t) &intr_0) + i * 0x10, 0x18, 0, 0x8e + (i == 42 ? (3 << 5) : 0));

	/* Load the IDT register */
	idt_ptr.lidt();
}
