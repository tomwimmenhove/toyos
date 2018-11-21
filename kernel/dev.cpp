#include "dev.h"
#include "console.h"
#include "new.h"

std::shared_ptr<io_handle> devices::open(int dev_type, int dev_idx)
{   
	con << "Trying to open " << dev_type << ':' << dev_idx << '\n';

	for(auto& dev: dev_list)
		if (dev->dev_type == dev_type)
			return dev->open(dev_idx);

	return nullptr;
}

