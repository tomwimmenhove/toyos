#ifndef DEV_H
#define DEV_H

#include <stdint.h>
#include <stddef.h>

#include <memory>

struct driver_handle
{
	int dev_type;

	virtual size_t read(int dev_idx, void* buf, size_t len) = 0;
	virtual size_t write(int dev_idx, void* buf, size_t len) = 0;
	virtual bool open(int dev_idx) = 0;
	virtual bool close(int dev_idx) = 0;

	std::shared_ptr<driver_handle> next;
};

class devices
{
public:
	devices()
	{ }

	void add(std::shared_ptr<driver_handle> handle);
	std::shared_ptr<driver_handle> open(int dev_type, int dev_idx);

private:
	std::shared_ptr<driver_handle> dev_head;
};



#endif /* DEV_H */
