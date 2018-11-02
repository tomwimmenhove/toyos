#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#include "multiboot2.h"

#define ALLOC_START             0x0000000000008000llu
#define KERNEL_STACK_TOP        0xfffff00000000000llu

#define KERNEL_ALLOC_START	0x0000000000200000llu // XXX: ok?


/* No longer at pml4[511], but at 510, so now we can't simply have all the
 * upper bits high, but instead the indices will be 510, so
 *
 *  0b1111111111111111 111111111 111111111 111111111 111111111 000000000000 // 0xfffffffffffff000
 * Becomes:
 *  0b1111111111111111 111111110 111111110 111111110 111111110 000000000000 // 0xffffff7fbfdfe000
 *  ... etc
 */
#define PG_PML4E		510
#define PG_PML4			0xffffff7fbfdfe000llu
#define PG_PDP			0xffffff7fbfc00000llu
#define PG_PD			0xffffff7f80000000llu
#define PG_PT			0xffffff0000000000llu

#define KBI_MAGIC		0xdeadbeef
typedef struct __attribute__((packed))
{
	uint32_t magic;

	uint64_t alloc_first;
	uint64_t alloc_end;

	uint64_t mb_tag;
} kernel_boot_info;

#endif /* CONFIG_H */
