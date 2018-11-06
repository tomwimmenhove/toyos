#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include "io.h"

template<uint8_t IO>
struct pic
{
	enum icw4_type
	{
		buf_none = 0x0,
		buf_slave = 0x1,
		buf_master = 0x3,
	};

	pic(bool ltim, bool adi, bool single, uint8_t vector, uint8_t slave_msk_dev)
	{
		icw1(ltim, adi, single, false);
		write_data(vector);
		write_data(slave_msk_dev);
	}

	pic(bool ltim, bool adi, bool single, uint8_t vector, uint8_t slave_msk_dev,
			bool auto_eoi, icw4_type type, bool sfnm)
	{
		icw1(ltim, adi, single, false);
		write_data(vector);
		write_data(slave_msk_dev);
		icw4(auto_eoi, type, sfnm);
	}

	inline void set_mask(uint8_t mask) { write_data(mask); }
	inline uint8_t get_mask() { return read_data(); }
	/*
	 * ... Implement OCW2, OCW3 if needed...
	 */

	inline void eoi() { command(0x20); }

private:
	inline void command(uint8_t cmd) { outb_p(cmd, IO); }
	inline void write_data(uint8_t write_data) { outb_p(write_data, IO + 1); }
	inline uint8_t read_data() { return inb_p(IO + 1); }

	inline void icw1(bool ltim, bool adi, bool single, bool ic4)
	{
		command(0x10 | (ltim ? 0x08 : 0) | (adi? 0x04 : 0) | (single? 0x02 : 0) | (ic4 ? 0x01 : 0));
	}

	inline void icw4(bool auto_eoi, icw4_type type, bool sfnm)
	{
		write_data((sfnm ? 0x10 : 0)) | (type << 2) | (auto_eoi ? 0x02 : 0);
	}
};

struct at_pic_sys
{
	void disable(uint8_t intr)
	{
		if (intr < 0x28)
			pic1.set_mask(pic1.get_mask() | (1 << (intr - 0x20)));
		else
			pic2.set_mask(pic2.get_mask() | (1 << (intr - 0x28)));
	}

	void enable(uint8_t intr)
	{
		if (intr < 0x28)
			pic1.set_mask(pic1.get_mask() & ~(1 << (intr - 0x20)));
		else
			pic2.set_mask(pic2.get_mask() & ~(1 << (intr - 0x28)));
	}

	void eoi(int irq_num);

private:
	pic<0x20> pic1
	{
		false, false, false,
		0x20,	/* Vector */
		0x04	/* Slave on pin 2 (1 << 2) */
	};

	pic<0xa0> pic2
	{
		false, false, false,
		0x28,	/* Vector */
		0x02	/* Slave ID 2 */
	};
};

extern at_pic_sys pic_sys;

#endif /* PIC_H */
