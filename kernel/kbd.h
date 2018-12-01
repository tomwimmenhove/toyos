#ifndef KBD_H
#define KBD_H

#include <stdint.h>
#include <stddef.h>
#include <memory>

#include "dev.h" 

struct io_tty : public io_handle
{   
	size_t read(void* buf, size_t len) override;
	size_t write(void* buf, size_t len) override;
	size_t seek(size_t) override { return -1; }

	bool close() override { return true; }
};

struct driver_tty : public driver
{
	std::shared_ptr<io_handle> open(int dev_idx);
};

void kbd_init();

#endif /* KBD_H */
