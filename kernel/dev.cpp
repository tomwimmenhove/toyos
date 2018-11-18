#include <stdint.h>

#include "dev.h"
#include "debug.h"
#include "new.h"

void devices::add(std::shared_ptr<driver_handle> handle)
{   
	auto head = dev_head;

	dev_head = handle;
	dev_head->next = head;
}

std::shared_ptr<driver_handle> devices::open(int dev_type, int dev_idx)
{   
	auto head = dev_head;

	con << "Trying to open " << dev_type << ':' << dev_idx << '\n';

	while (head)
	{   
		if (head->dev_type == dev_type)
		{   
			if (head->open(dev_idx))
				return head;
			else
				return nullptr;
		}

		head = head->next;
	}
	return nullptr;
}

