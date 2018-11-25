#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <assert.h>

#include "spinlock.h"
#include "req_queue.h"
#include "semaphore.h"

#if 0
struct disk_request
{   
	enum class type
	{   
		read,
		write
	};

	type t;
	void* buf;
	uint64_t blk_first;
	int blk_cnt;
	int disk_id;

	//embxx::util::StaticFunction<void()> callback;
	semaphore* sem;

	disk_request* next;
};
#endif

struct disk_block_io
{
    virtual void read(void* buffer, uint64_t blk_first, int blk_cnt, embxx::util::StaticFunction<void()> callback) = 0;
	virtual void write(void* buffer, uint64_t blK_first, int blk_cnt, embxx::util::StaticFunction<void()> callback) = 0;
};

#endif /* DISK_H */
