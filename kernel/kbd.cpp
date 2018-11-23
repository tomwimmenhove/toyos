#include <embxx/container/StaticQueue.h>

#include "kbd.h"
#include "console.h"
#include "task.h"
#include "cache_alloc.h"
#include "pic.h"
#include "interrupts.h"

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
		char ch = key_queue.back();
		b[t] = ch;
		key_queue.pop_back();

		/* echo */
		con << ch;

		t++;
		len--;

		if (ch == '\n')
			break;
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

struct keyboard : public intr_driver
{
	keyboard()
	 : intr_driver(pic_sys.to_intr(1)),
	   caps(false), num(false), scroll(false),
	   lshift(false), lalt(false), lctrl(false),
	   rshift(false), ralt(false), rctrl(false)
	{
		for (int i = 0; i < 0x80; i++)
			key_map[i] = 0;

		key_map[0x01] = kbd_escape;
		key_map[0x02] = '1' | '!' << 8;
		key_map[0x03] = '2' | '@' << 8;
		key_map[0x04] = '3' | '#' << 8;
		key_map[0x05] = '4' | '$' << 8;
		key_map[0x06] = '5' | '%' << 8;
		key_map[0x07] = '6' | '^' << 8;
		key_map[0x08] = '7' | '&' << 8;
		key_map[0x09] = '8' | '*' << 8;
		key_map[0x0A] = '9' | '(' << 8;
		key_map[0x0B] = '0' | ')' << 8;
		key_map[0x0C] = '-' | '_' << 8;
		key_map[0x0D] = '=' | '+' << 8;
		key_map[0x0E] = '\b';
		key_map[0x0F] = '\t';
		key_map[0x10] = 'Q';
		key_map[0x11] = 'W';
		key_map[0x12] = 'E';
		key_map[0x13] = 'R';
		key_map[0x14] = 'T';
		key_map[0x15] = 'Y';
		key_map[0x16] = 'U';
		key_map[0x17] = 'I';
		key_map[0x18] = 'O';
		key_map[0x19] = 'P';
		key_map[0x1A] = '[';
		key_map[0x1B] = ']';
		key_map[0x1C] = '\n';
		key_map[0x1D] = kbd_l_ctrl;
		key_map[0x1E] = 'A';
		key_map[0x1F] = 'S';
		key_map[0x20] = 'D';
		key_map[0x21] = 'F';
		key_map[0x22] = 'G';
		key_map[0x23] = 'H';
		key_map[0x24] = 'J';
		key_map[0x25] = 'K';
		key_map[0x26] = 'L';
		key_map[0x27] = ';' | ':' << 8;
		key_map[0x28] = '\'' | '"' << 8;
		key_map[0x29] = '`' | '~' << 8;
		key_map[0x2A] = kbd_l_shift;
		key_map[0x2B] = '\\' | '|' << 8;
		key_map[0x2C] = 'Z';
		key_map[0x2D] = 'X';
		key_map[0x2E] = 'C';
		key_map[0x2F] = 'V';
		key_map[0x30] = 'B';
		key_map[0x31] = 'N';
		key_map[0x32] = 'M';
		key_map[0x33] = ',' | '<' << 8;
		key_map[0x34] = '.' | '>' << 8;
		key_map[0x35] = '/' | '?' << 8;
		key_map[0x36] = kbd_r_shift;
		key_map[0x37] = '*' | kbd_np;
		key_map[0x38] = kbd_l_alt;
		key_map[0x39] = ' ';
		key_map[0x3A] = kbd_caps;
		key_map[0x3B] = kbd_f1;
		key_map[0x3C] = kbd_f2;
		key_map[0x3D] = kbd_f3;
		key_map[0x3E] = kbd_f4;
		key_map[0x3F] = kbd_f5;
		key_map[0x40] = kbd_f6;
		key_map[0x41] = kbd_f7;
		key_map[0x42] = kbd_f8;
		key_map[0x43] = kbd_f9;
		key_map[0x44] = kbd_f10;
		key_map[0x45] = kbd_numlock;
		key_map[0x46] = kbd_scrlock;
		key_map[0x47] = '7' | kbd_np;
		key_map[0x48] = '8' | kbd_np;
		key_map[0x49] = '9' | kbd_np;
		key_map[0x4A] = '-' | kbd_np;
		key_map[0x4B] = '4' | kbd_np;
		key_map[0x4C] = '5' | kbd_np;
		key_map[0x4D] = '6' | kbd_np;
		key_map[0x4E] = '+' | kbd_np;
		key_map[0x4F] = '1' | kbd_np;
		key_map[0x50] = '2' | kbd_np;
		key_map[0x51] = '3' | kbd_np;
		key_map[0x52] = '0' | kbd_np;
		key_map[0x53] = '.' | kbd_np;
		key_map[0x57] = kbd_f11;
		key_map[0x58] = kbd_f12;
	}

	void update_leds()
	{
		outb_p(0xed, 0x60);
		outb_p((caps ? 4 : 0) | (num ? 2 : 0) | (scroll ? 1 : 0), 0x60);
		// XXX: Don't know if this works. Qemu doesn't pass through the keyboard LEDs.
	}

	void key_down(uint32_t code)
	{
		if (code == kbd_l_shift) { lshift = true; return; }
		if (code == kbd_r_shift) { rshift = true; return; }

		if (code == kbd_l_ctrl) { lctrl = true; return; }
		if (code == kbd_r_ctrl) { rctrl = true; return; }

		if (code == kbd_l_alt) { lalt = true; return; }
		if (code == kbd_r_alt) { ralt = true; return; }

		if (code == kbd_caps)
		{
			caps = !caps;
			update_leds();
			return;
		}

		if ( (code & lalt) || (code & ralt) )
			return;

		char ch = 0;
		if (code >= 'A' && code <= 'Z')
			ch = lctrl || rctrl || (caps ^ (lshift || rshift)) ? code : code ^ 0x20;
		else if (code & 0xff) /* Key is a character */
		{
			if ((code & 0xff00) && (lshift || rshift)) /* Does the key have a 'shifted' brother while we're currently shifted? */
				ch = (code >> 8) & 0xff;
			else
				ch = code & 0xff;
		}

		if (lctrl || rctrl)
			ch -= 0x40;

		key_queue.push_back(ch);
	}

	void key_up(uint32_t code)
	{
		if (code == kbd_l_shift) { lshift = false; return; }
		if (code == kbd_r_shift) { rshift = false; return; }

		if (code == kbd_l_ctrl) { lctrl = false; return; }
		if (code == kbd_r_ctrl) { rctrl = false; return; }

		if (code == kbd_l_alt) { lalt = false; return; }
		if (code == kbd_r_alt) { ralt = false; return; }
	}

	void interrupt(uint64_t, interrupt_state*) override
	{
		auto b = inb_p(0x60);
		if (b < 0x80)
			key_down(key_map[b]);
		else
			key_up(key_map[b - 0x80]);
	}

private:
	bool caps:1;
	bool num:1;
	bool scroll:1;
	bool lshift:1;
	bool lalt:1;
	bool lctrl:1;
	bool rshift:1;
	bool ralt:1;
	bool rctrl:1;

	uint32_t key_map[0x80];

	static constexpr uint32_t kbd_escape  = 0x00010000;;
	static constexpr uint32_t kbd_caps    = 0x00020000;;
	static constexpr uint32_t kbd_f1      = 0x00030000;;
	static constexpr uint32_t kbd_f2      = 0x00040000;;
	static constexpr uint32_t kbd_f3      = 0x00050000;;
	static constexpr uint32_t kbd_f4      = 0x00060000;;
	static constexpr uint32_t kbd_f5      = 0x00070000;;
	static constexpr uint32_t kbd_f6      = 0x00080000;;
	static constexpr uint32_t kbd_f7      = 0x00090000;;
	static constexpr uint32_t kbd_f8      = 0x000a0000;;
	static constexpr uint32_t kbd_f9      = 0x000b0000;;
	static constexpr uint32_t kbd_f10     = 0x000c0000;;
	static constexpr uint32_t kbd_f11     = 0x000d0000;;
	static constexpr uint32_t kbd_f12     = 0x000e0000;;
	static constexpr uint32_t kbd_numlock = 0x000f0000;;
	static constexpr uint32_t kbd_scrlock = 0x00100000;;

	static constexpr uint32_t kbd_np      = 0x81000000;
	static constexpr uint32_t kbd_l_shift = 0x82000000;;
	static constexpr uint32_t kbd_r_shift = 0x84000000;;
	static constexpr uint32_t kbd_l_ctrl  = 0x88000000;;
	static constexpr uint32_t kbd_r_ctrl  = 0x90000000;;
	static constexpr uint32_t kbd_l_alt   = 0xa0000000;;
	static constexpr uint32_t kbd_r_alt   = 0xc0000000;;
};

static keyboard* kbd;

void kbd_init()
{
	kbd = new keyboard();
}

