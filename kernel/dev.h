#ifndef DEV_H
#define DEV_H

#include <stdint.h>
#include <stddef.h>
#include <forward_list>

#include <memory>

struct driver_handle
{
	int dev_type;

	virtual size_t read(void* buf, size_t len) = 0;
	virtual size_t write(void* buf, size_t len) = 0;
	virtual bool open(int dev_idx) = 0;
	virtual bool close(int dev_idx) = 0;
};

class devices
{
public:
	devices()
	{ }

	void add(std::shared_ptr<driver_handle> handle) { dev_head.push_front(handle); }
	std::shared_ptr<driver_handle> open(int dev_type, int dev_idx);

private:
	std::forward_list<std::shared_ptr<driver_handle>> dev_head;
};



#endif /* DEV_H */
