#ifndef DESCRIPTORS_H
#define DESCRIPTORS_H

#include <stdint.h>
#include "tss.h"
#include "config.h"

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
struct __attribute__((packed)) gdt_entry { };

struct __attribute__((packed)) gdt128_entry : public gdt_entry
{
	uint64_t l0;
	uint64_t l1;
};

struct __attribute__((packed)) gdt64_entry : public gdt_entry
{
	uint64_t l;
};

/* NULL descriptor entry */
struct __attribute__((packed)) desc_null : public gdt64_entry
{
	desc_null()
		: gdt64_entry { {}, 0 }
	{ }
};

/* THESE NEED CLEANING UP! THIS IS RIDICULOUS! */

/* Code segment descriptor */
struct __attribute__((packed)) desc_code_seg : public gdt64_entry
{
	desc_code_seg(bool conforming, int privilege, bool present, bool long_mode, bool large_operand)
		: gdt64_entry{ {},    ((uint64_t) conforming ? 1llu << (32 + 10) : 0) |
			((uint64_t) (privilege & 3) << (32 + 13)) |
				((uint64_t) present ? 1llu << (32 + 15) : 0) |
				((uint64_t) long_mode ? 1llu << (32 + 21) : 0) |
				((uint64_t) large_operand ? 1llu << (32 + 22) : 0) |
				((uint64_t) 3llu << (32 + 11)) }
	{ }
};

/* Data segment descriptor */
struct __attribute__((packed)) desc_data_seg : public gdt64_entry
{
	desc_data_seg(uint32_t base, bool writable, int privilege, bool present)
		: gdt64_entry{ {},   ((uint64_t) (base & 0xffff) << 16) |
			((uint64_t) ((base >> 16) & 0xff) << 32) |
				((uint64_t) ((base >> 24) & 0xff) << 56) |
				((uint64_t) writable ? 1llu << (32 + 9) : 0) |
				((uint64_t) (privilege & 3) << (32 + 13)) |
				((uint64_t) present ? 1llu << (32 + 15) : 0) |
				((uint64_t) 1llu << (32 + 12))}
	{ }
};

/* TSS descriptor */
struct __attribute__((packed)) desc_tss : public gdt128_entry
{
	desc_tss(tss* t, int privilege, bool present)
		: gdt128_entry{ {}, 

			/* Lower 64 bits */
			((uint64_t) ((uint64_t) t & 0xffff) << 16) |
				((uint64_t) (((uint64_t) t >> 16) & 0xff) << 32) |
				((uint64_t) (((uint64_t) t >> 24) & 0xff) << 56) |
				((uint64_t) (privilege & 3) << (32 + 13)) |
				((uint64_t) present ? 1llu << (32 + 15) : 0) |

				// 64-bit  TSS  (Available)
				((uint64_t) 1llu << (32 + 8)) |
				((uint64_t) 1llu << (32 + 11)) |

				(sizeof(tss) - 1)
			, 
			/* Upper 64 bits */
			((uint64_t) t) >> 32
		}
	{ }
};

inline void ltr(uint16_t offs)
{
	 asm volatile("ltr %0"
			 : : "r" (offs));
}


/* General descriptor pointer */
struct __attribute__((packed)) desc_ptr
{
	desc_ptr(gdt_entry* base, uint16_t size)
		: size(size - 1), base(base)
	{ }

	desc_ptr(idt_entry* base, uint16_t size)
		: size(size - 1), base(base)
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
