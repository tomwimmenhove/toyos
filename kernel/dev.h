#ifndef DEV_H
#define DEV_H

#include <stdint.h>
#include <stddef.h>
#include <forward_list>
#include <memory>
#include <sys/types.h>

struct io_handle
{
	virtual ssize_t read(void* buf, size_t len) = 0;
	virtual ssize_t write(void* buf, size_t len) = 0;
	virtual size_t seek(size_t pos) = 0;
	virtual size_t size() = 0;

	virtual ~io_handle()
	{ }
};

struct driver
{
	int dev_type;

	virtual std::shared_ptr<io_handle> open(int dev_idx) = 0;
};

class devices
{
public:
	devices()
	{ }

	void add(std::shared_ptr<driver> handle) { dev_list.push_front(handle); }
	std::shared_ptr<io_handle> open(int dev_type, int dev_idx);

private:
	std::forward_list<std::shared_ptr<driver>> dev_list;
};



#endif /* DEV_H */
