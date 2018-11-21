#ifndef DEV_H
#define DEV_H

#include <stdint.h>
#include <stddef.h>
#include <forward_list>

#include <memory>

struct io_handle
{
	virtual size_t read(void* buf, size_t len) = 0;
	virtual size_t write(void* buf, size_t len) = 0;
	virtual bool close() = 0;
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
