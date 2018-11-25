#ifndef REQ_QUEUE
#define REQ_QUEUE

#include <stdint.h>
#include <assert.h>

#include "spinlock.h"

template<typename T>
struct is_slist : std::integral_constant<bool,
	requires(T t) { { t.next } -> T*; }>
{};

template<typename T>
	requires is_slist<T>::value
struct req_queue
{
	inline void enqueue(T* item)
	{
		if (!head)
		{
			spinlock lock(&li);
			head = tail = item;
			return;
		}
		assert(tail);

		spinlock lock(&li);
		tail->next = item;
		item->next = nullptr;
		tail = item;
	}
	
	inline void dequeue()
	{
		assert(head);

		spinlock lock(&li);
		head = head->next;
	}

	inline T* first() { return head; }

private:
	T* head = nullptr;
	T* tail = nullptr;

	int li = 0;
};

#endif /* REQ_QUEUE */
