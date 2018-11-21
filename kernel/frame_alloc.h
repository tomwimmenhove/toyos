#ifndef FRAME_ALLOC_H
#define FRAME_ALLOC_H

#include <stdint.h>
#include <stddef.h>

#include "mem.h"
#include "linker.h"
#include "debug.h"

/* Frame allocator interface */
class frame_alloc_iface
{
public: 
	virtual uint64_t page() = 0;
	virtual void free(uint64_t addr) = 0;
};

/* Dump frame allocater to be used before we can usue our 'actual' bitmap-based allocator */
class frame_alloc_dumb : public frame_alloc_iface
{
public: 
	frame_alloc_dumb(kernel_boot_info* kbi, uint64_t alloc_ptr_start, uint64_t alloc_ptr_end)
		: kbi(kbi), alloc_ptr(alloc_ptr_start), alloc_ptr_end(alloc_ptr_end)
	{ }

	uint64_t page() override;
	void free(uint64_t addr) override { (void) addr; } /* Doesn't do anything. */

private:
	kernel_boot_info* kbi;
	uint64_t alloc_ptr;
	uint64_t alloc_ptr_end;
};

/* Our 'actual' bitmap allocator */
class frame_alloc_bitmap : public frame_alloc_iface
{
public:
	frame_alloc_bitmap(size_t size)
		: size(size)
	{ }

	int at(size_t p);
	void set(size_t p);
	void set_range(size_t begin, size_t end);
	void reset(size_t p);
	void reset_range(size_t begin, size_t end);
	size_t find_zero();
	uint64_t page() override;
	void free(uint64_t addr) override { reset(addr / PAGE_SIZE / 8); }
	uint64_t mem_free();

private:
	void check_index(size_t p);

	uint64_t last_zero = 0;
	size_t size;
	uint8_t data[0];
};

#endif /* FRAME_ALLOC_H */
