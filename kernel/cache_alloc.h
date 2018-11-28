#ifndef CACHE_ALLOC
#define CACHE_ALLOC

#include <assert.h>
#include <memory>
#include <utility>
#include <forward_list>

#include "debug.h"

template<typename T>
struct cache_alloc
{
	template<typename... Args>
	static T* take(Args&&... args)
	{
		for(auto& entry: list)
		{
			//if (!entry.used) // need volatile?
			/* XXX: LOCK */
			if (!entry.used)
			{
				entry.used = 1;
				return new(&entry.stor) T(std::forward<Args>(args)...);;
			}
			/* XXX: UNLOCK */
		}

		/* XXX: LOCK */
		list.emplace_front(std::forward<Args>(args)...);
		auto& front = list.front();
		/* XXX: UNLOCK */

		return &front.stor;
	}

	static void release(T* p)
	{
		//con << "release " << hex_u64((uint64_t) p) << '\n';
		for(auto& entry: list)
		{
			if (&entry.stor == p)
			{
				assert(entry.used);
				entry.used = false;
				entry.stor.~T();

				return;
			}
		}

		panic("tried to release a non-existing cache entry");
	}

	template<typename... Args>
	static std::shared_ptr<T> take_shared(Args&&... args)
	{
		return std::shared_ptr<T>(take(std::forward<Args>(args)...), release );
	}

private:
	struct cache
	{
		template<typename... Args>
		cache(Args&&... args)
		 : stor(std::forward<Args>(args)...), used(true)
		{ }

		T stor;
		bool used;
	};

	static std::forward_list<cache> list;
};

template<typename T>
std::forward_list<typename cache_alloc<T>::cache> cache_alloc<T>::list;

#endif /* CACHE_ALLOC */
