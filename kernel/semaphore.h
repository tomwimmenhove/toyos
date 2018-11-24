#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "task.h"

extern std::shared_ptr<task> current;
void schedule();

struct semaphore
{   
	void operator--()
	{   
		current->wait_for = [this]() { return n != 0; };
		
		while (n <= 0)
			schedule();
		n--;

		current->wait_for = nullptr;
	}

	void operator++()
	{   
		n++;
	}

private:
	int lock_i = 0;
	volatile int n = 0;
};

#endif /* SEMAPHORE_H */
