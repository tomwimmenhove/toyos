#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include "io.h"

template<uint8_t IO>
struct pic
{
	inline void command(uint8_t cmd) { outb_p(cmd, IO); }
	inline void data(uint8_t data) { outb_p(data, IO + 1); }
	inline void eoi() { command(0x20); }
};

struct at_pic_sys
{
	    at_pic_sys();
		void eoi(int irq_num);

		pic<0x20> pic1;
		pic<0xa0> pic2;
};

extern at_pic_sys pic_sys;

#endif /* PIC_H */
