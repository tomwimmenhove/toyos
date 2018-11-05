#include "pic.h"

#include <stdint.h>
#include "io.h"

at_pic_sys::at_pic_sys()
{   
	pic1.command(0x11);
	pic2.command(0x11);

	pic1.data(0x20);
	pic2.data(0x28);

	pic1.data(0x04);
	pic2.data(0x02);
}

void at_pic_sys::eoi(int irq_num)
{   
	if (irq_num >= 0x20 && irq_num < 0x30)
	{   
		if (irq_num >= 0x28)
			pic2.eoi();

		pic1.eoi();
	}
}

at_pic_sys pic_sys;

