#ifndef DISK_H
#define DISK_H

struct disk_request
{   
	enum class type
	{   
		read,
		write
	};

	type t;
	void* buf;
	uint64_t sect_start;
	uint64_t sect_cnt;
	bool slave;
	embxx::util::StaticFunction<void()> callback;
};

struct disk_block_io
{
    virtual void read(void* buffer, uint64_t blk_first, int blk_cnt, embxx::util::StaticFunction<void()> callback) = 0;
	virtual void write(void* buffer, uint64_t blK_FIRst, int blk_cnt, embxx::util::StaticFunction<void()> callback) = 0;
};

#endif /* DISK_H */
