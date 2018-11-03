#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

extern "C"
{
	#include "../common/debug_out.h"
	#include "../common/config.h"
}

class frame_alloc_iface;
class frame_alloc_bitmap;

class memory;
extern memory* mem;

void die();

class memory
{
public: 
	memory(kernel_boot_info* kbi);

	static inline void init(memory* m) { mem = m; }

	void unmap_page(uint64_t virt);
	void map_page(uint64_t virt, uint64_t phys);

	frame_alloc_iface* frame_alloc;

	static uint64_t get_phys(uint64_t virt);

private:
	void setup_usage(multiboot_tag_mmap* mb_mmap_tag, uint64_t memtop);
	multiboot_tag_mmap* get_mem_map(kernel_boot_info* kbi);
	void clean_page_tables();

	inline frame_alloc_bitmap* get_bitmap() { return reinterpret_cast<frame_alloc_bitmap*>(frame_alloc); }

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

