#ifndef DISK_H
#define DISK_H

#include <stdint.h>

struct disk_block_io
{
	virtual void read(void* buffer, size_t pos, size_t cnt) = 0;
	virtual void write(void* buffer, size_t pos, size_t cnt) = 0;
};

#endif /* DISK_H */
