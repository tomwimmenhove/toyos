#include "pic.h"

#include <stdint.h>
#include "io.h"

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

