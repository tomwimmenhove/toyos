#include "dev.h"
#include "console.h"
#include "new.h"

std::shared_ptr<driver_handle> devices::open(int dev_type, int dev_idx)
{   
	con << "Trying to open " << dev_type << ':' << dev_idx << '\n';

	for(auto& handle: dev_head)
	{   
		if (handle->dev_type == dev_type)
		{   
			if (handle->open(dev_idx))
				return handle;
			else
				return nullptr;
		}
	}
	return nullptr;
}

