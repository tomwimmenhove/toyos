#ifndef DESCRIPTORS_H
#define DESCRIPTORS_H

#include <stdint.h>

/* IDT entry */
struct __attribute__((packed)) idt_entry
{
	uint16_t offset_1; 
	uint16_t selector; 
	uint8_t ist;
	uint8_t type_attr; 
	uint16_t offset_2; 
	uint32_t offset_3; 
	uint32_t zero;     

	idt_entry()
	{ }

	idt_entry(uint64_t offset, uint16_t selector, uint8_t ist, uint8_t type_attr)
		: offset_1(offset & 0xffff),
		selector(selector),
		ist(ist),
		type_attr(type_attr),
		offset_2((offset >> 16) & 0xffff),
		offset_3((offset >> 32) & 0xffffffff),
		zero(0)
	{ }
};

/* GDT entry */
struct __attribute__((packed)) gdt_entry
{
	uint64_t l;
};

/* NULL descriptor entry */
struct __attribute__((packed)) desc_null : public gdt_entry
{
	desc_null()
		: gdt_entry{0}
	{ }
};

/* Code segment descriptor */
struct __attribute__((packed)) desc_code_seg : public gdt_entry
{
	desc_code_seg(bool conforming, int privilege, bool present, bool long_mode, bool large_operand)
		: gdt_entry{    ((uint64_t) conforming ? 1llu << (32 + 10) : 0) |
			((uint64_t) (privilege & 3) << (32 + 13)) |
				((uint64_t) present ? 1llu << (32 + 15) : 0) |
				((uint64_t) long_mode ? 1llu << (32 + 21) : 0) |
				((uint64_t) large_operand ? 1llu << (32 + 22) : 0) |
				((uint64_t) 3llu << (32 + 11)) }
	{ }
};

/* Data segment descriptor */
struct __attribute__((packed)) desc_data_seg : public gdt_entry
{
	desc_data_seg(uint32_t base, bool writable, int privilege, bool present)
		: gdt_entry{    ((uint64_t) (base & 0xffff) << 16) |
			((uint64_t) ((base >> 16) & 0xff) << 32) |
				((uint64_t) ((base >> 24) & 0xff) << 56) |
				((uint64_t) writable ? 1llu << (32 + 9) : 0) |
				((uint64_t) (privilege & 3) << (32 + 13)) |
				((uint64_t) present ? 1llu << (32 + 15) : 0) |
				((uint64_t) 1llu << (32 + 12))}
	{ }
};

/* General descriptor pointer */
struct __attribute__((packed)) desc_ptr
{
	desc_ptr(gdt_entry* base, uint16_t size)
		: size(size), base(base)
	{ }

	desc_ptr(idt_entry* base, uint16_t size)
		: size(size), base(base)
	{ }

	inline void lgdt()
	{
		asm volatile(   "lgdt %0"
				: : "m" (*this));
	}

	inline void lidt()
	{
		asm volatile(   "lidt %0"
				: : "m" (*this));
	}

	uint16_t size;
	void* base;
};

#endif /* DESCRIPTORS_H */
