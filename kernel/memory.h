#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <cstddef>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/config.h"
}

class frame_alloc_iface
{
public: 
	uint64_t virtual page() = 0;
};

void die();

struct bitmap : public frame_alloc_iface
{
	bitmap(size_t size)
		: size(size)
	{ }

	inline int at(size_t p)
	{
		check_index(p);
		return data[p / 8] & (1 << (p % 8));
	}

	inline void set(size_t p)
	{
		check_index(p);
		data[p / 8] |= 1 << (p % 8);
	}

	inline void set_range(size_t begin, size_t end)
	{
		check_index(begin);
		check_index(end - 1);
		for (auto i = begin; i < end; i++)
			set(i);
	}

	inline void reset(size_t p)
	{
		check_index(p);
		data[p / 8] &= ~(1 << (p % 8));
	}

	inline void reset_range(size_t begin, size_t end)
	{
		check_index(begin);
		check_index(end - 1);
		for (auto i = begin; i < end; i++)
			reset(i);
	}

	inline size_t find_zero()
	{
		for (size_t i = 0 ; i < size; i++)
			if (!at(i))
				return i;
		return SIZE_MAX;
	}
	uint64_t page() override
	{
		uint64_t p = find_zero();
		set(p);
		return p * 0x1000;
	}

	uint64_t mem_free()
	{
		uint64_t tot = 0;
		for (size_t i = 0; i < size; i++)
			if (!at(i))
				tot += 0x1000;

		return tot;
	}

	private:
	inline void check_index(size_t p)
	{
		if (p >= size)
		{
			putstring("Tried to access past end of bitmap!\n");
			die();
		}
	}

	private:
	size_t size;
	uint8_t data[0];
};

class memory
{
public: 
	memory(kernel_boot_info* kbi);

	void unmap_page(uint64_t virt);
	void map_page(uint64_t virt, uint64_t phys);

	frame_alloc_iface* frame_alloc;

	static uint64_t get_phys(uint64_t virt);

private:
	void setup_usage(multiboot_tag_mmap* mb_mmap_tag, uint64_t memtop);
	multiboot_tag_mmap* get_mem_map(kernel_boot_info* kbi);
	void clean_page_tables();

	inline bitmap* get_bitmap() { return reinterpret_cast<bitmap*>(frame_alloc); }

	inline uint64_t cr3_get()
	{
		uint64_t ret;
		asm volatile(   "mov %%cr3 , %0"
				: "=r" (ret)
				: :);
		return ret;
	}

	inline void cr3_set(uint64_t cr)
	{
		asm volatile(   "mov %0, %%cr3"
				: : "r" (cr)
				:);
	}

	static void clear_page(void* page);
};

template<typename T>
class temp_page
{
public: 
	temp_page(uint64_t virt, uint64_t phys, memory* mem)
		: virt((T*) virt), mem(mem)
	{ mem->map_page((uint64_t) virt, phys); }

	~temp_page() { mem->unmap_page((uint64_t) virt); }

	T operator [] (int idx) const { return virt[idx]; }
	T& operator [] (int idx) { return virt[idx]; }

private:
	T* virt;
	memory* mem;
};


#endif /* MEMORY_H */

