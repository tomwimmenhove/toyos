#include "config.h"
#include "frame_alloc.h"
#include "mb.h"
#include "linker.h"

/* frame_alloc_dumb */
uint64_t frame_alloc_dumb::page()
{
	uint64_t p = alloc_ptr;
	while ((p >= kbi->alloc_first && p < kbi->alloc_end) ||
			(p >= memory::get_phys((uint64_t) &_code_start) && p <= memory::get_phys((uint64_t) &_data_end - 0x1000)))
		p += 0x1000;

	alloc_ptr = p + 0x1000;

	if (p >= alloc_ptr_end)
		return 0;

	return p;
}

/* frame_alloc_bitmap::at */
int frame_alloc_bitmap::at(size_t p)
{
	check_index(p);
	return data[p / 8] & (1 << (p % 8));
}

void frame_alloc_bitmap::set(size_t p)
{
	check_index(p);
	data[p / 8] |= 1 << (p % 8);
}

void frame_alloc_bitmap::set_range(size_t begin, size_t end)
{
	check_index(begin);
	check_index(end - 1);
	for (auto i = begin; i < end; i++)
		set(i);
}

void frame_alloc_bitmap::reset(size_t p)
{
	check_index(p);
	data[p / 8] &= ~(1 << (p % 8));
}

void frame_alloc_bitmap::reset_range(size_t begin, size_t end)
{
	check_index(begin);
	check_index(end - 1);
	for (auto i = begin; i < end; i++)
		reset(i);
}
size_t frame_alloc_bitmap::find_zero()
{
	uint64_t j = last_zero;
	for (size_t i = 0 ; i < size; i++)
	{
		j++;
		if (j >= size)
			j = 0;
		if (!at(j))
		{
			last_zero = j;
			return j;
		}
	}
	return SIZE_MAX;
}
uint64_t frame_alloc_bitmap::page()
{
	uint64_t p = find_zero();
	if (p == SIZE_MAX)
		panic("OOM during frame_alloc_bitmap::page()");
	set(p);
	return p * 0x1000;
}

uint64_t frame_alloc_bitmap::mem_free()
{
	uint64_t tot = 0;
	for (size_t i = 0; i < size; i++)
		if (!at(i))
			tot += 0x1000;

	return tot;
}

void frame_alloc_bitmap::check_index(size_t p)
{
	if (p >= size)
		panic("Tried to access past end of frame_alloc_bitmap!");
}

