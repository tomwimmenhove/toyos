#ifndef MB_H
#define MB_H

extern "C"
{
#include "../common/multiboot2.h"
}

template<typename T, typename U>
static inline T mb_next(T entry, U tag)
{
	return (T) ((uint64_t) entry + (((U) tag)->entry_size));
}


#endif /* MB_H */
