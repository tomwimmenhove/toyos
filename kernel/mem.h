#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include "interrupts.h"

extern "C"
{
	#include "../common/config.h"
}

#include "debug.h"
#include "sysregs.h"

class frame_alloc_iface;
class frame_alloc_bitmap;

class memory
{
public: 
	static void init(kernel_boot_info* kbi);
	static void unmap_unused();
	static uint64_t is_mapped(uint64_t virt);
	static void unmap_page(uint64_t virt);
	static void map_page(uint64_t virt, uint64_t phys);
	static uint64_t get_phys(uint64_t virt);

	static void handle_pg_fault(interrupt_state* state, uint64_t addr);

	static frame_alloc_iface* frame_alloc;
private:
	static void setup_usage(multiboot_tag_mmap* mb_mmap_tag, uint64_t memtop);
	static void clean_page_tables();

	static inline frame_alloc_bitmap* get_bitmap() { return reinterpret_cast<frame_alloc_bitmap*>(frame_alloc); }
	static void clear_page(void* page);
};

template<typename T>
class temp_page
{
public: 
	temp_page(uint64_t virt, uint64_t phys)
		: virt((T*) virt)
	{ memory::map_page((uint64_t) virt, phys); }

	~temp_page() { memory::unmap_page((uint64_t) virt); }

	T operator [] (int idx) const { return virt[idx]; }
	T& operator [] (int idx) { return virt[idx]; }

private:
	T* virt;
};


#endif /* MEMORY_H */

