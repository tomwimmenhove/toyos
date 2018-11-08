#ifndef MB_H
#define MB_H

extern "C"
{
#include "../common/multiboot2.h"
}
#include "config.h"

class mb
{
public:
	template<typename T>
	static inline T get_tag(kernel_boot_info* kbi, uint32_t tag_type)
	{
		auto mb_tag_addr = kbi->mb_tag;
		auto mb_tag = (multiboot_tag*) mb_tag_addr;
		while (mb_tag->type)
		{   
			if (mb_tag->type == tag_type)
				return (T) mb_tag;
			mb_tag_addr += (mb_tag->size + 7) & ~7;
			mb_tag = (multiboot_tag *) mb_tag_addr;
		}

		return nullptr;
	}

	template<typename T, typename U>
	static inline T next(T entry, U tag)
	{
		return (T) ((uint64_t) entry + (((U) tag)->entry_size));
	}
};


#endif /* MB_H */
