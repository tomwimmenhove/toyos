#include <embxx/container/StaticQueue.h>

#include "kbd.h"
#include "console.h"
#include "task.h"
#include "cache_alloc.h"
#include "pic.h"

extern std::shared_ptr<task> current;

static embxx::container::StaticQueue<uint8_t, 4096> key_queue;

size_t io_tty::read(void* buf, size_t len)
{   
	current->wait_for = []() { return key_queue.size() != 0; };

	uint8_t* b = (uint8_t*) buf;

	size_t t = 0;
	while (len)
	{   
		/* Wait until data is available */
		if (!key_queue.size())
			schedule();

		/* XXX: LOCK SHIT HERE */
		b[t] = key_queue.back();
		key_queue.pop_back();

		t++;
		len--;
	}

	/* No longer waiting */
	current->wait_for = nullptr;

	return t;
}

size_t io_tty::write(void* buf, size_t len)
{   
	con.write_buf((const char*) buf, len);

	return len;
}

std::shared_ptr<io_handle> driver_tty::open(int dev_idx)
{   
	if (dev_idx != 0)
	{   
		con << "Trying to open a non-zero tty\n";
		return nullptr;
	}

	return cache_alloc<io_tty>::take_shared();
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);

	key_queue.push_back(ch);
}

void kbd_init()
{
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
}
