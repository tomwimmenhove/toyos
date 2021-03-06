#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct __attribute__((packed)) gdt_ptr
{
        uint16_t size;
        uint64_t* base;
};

/* Figure 4-13 in AMD64 Architecture Programmer’s Manual, Volume 2: System Programming */
static inline uint64_t mk_desc(uint32_t limit, uint32_t base, int type, int s, int privilege, int present, int long_mode, int size_db, int granularity)
{
	uint32_t ldw;
	uint32_t udw;

	ldw = (limit & 0xffff) | ((base & 0xffff) << 16);
	udw = ((base >> 16) & 0xff) | (type << 8) | (s << 12) | (privilege << 13) | (present << 15) | (limit & 0xf0000) | (long_mode << 21) | (size_db << 22) | (granularity << 23) | (base & 0xff000000);

	return (uint64_t) udw << 32 | ldw;
}

static inline void lgdt(struct gdt_ptr* p)
{
        asm volatile(   "lgdt %0"
                        : 
                        : "m" (*p));
}

#endif /* GDT_H */
