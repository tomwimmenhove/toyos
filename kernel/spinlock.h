#ifndef SPINLOCK_H
#define SPINLOCK_H

struct spinlock
{
	spinlock(int volatile* p)
		: p(p)
	{   
		while(!__sync_bool_compare_and_swap(p, 0, 1))
			while(*p)
				asm volatile("pause");

		__sync_synchronize();
	}

	~spinlock()
	{   
		*p = 0;

		__sync_synchronize();
	}

private:
	int volatile* p;
};


#endif /* SPINLOCK_H */
