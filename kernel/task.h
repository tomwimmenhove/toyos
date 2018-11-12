#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

#include "klib.h"
#include "task_helper.h"

struct wait_fn_call_base
{
	virtual bool operator()() = 0;
	virtual ~wait_fn_call_base() {}
};

template <typename F>
struct wait_fn_call : wait_fn_call_base
{
	wait_fn_call(F functor) : functor(functor) {}
	virtual bool operator()() { return functor(); }
private:
	F functor;
};

template<int S>
struct wait_fn_stor
{
private:
	uint8_t buf[S];
};

template<int S = (sizeof(void*) * 3)>
class wait_fn
{
public:
	inline wait_fn()
		: stor_ptr(nullptr)
	{ }

	template <typename F>
	inline wait_fn(F f) { reset(f); }

	template <typename F>
	inline void reset(F f)
	{   
		static_assert(sizeof(stor) >= sizeof(wait_fn_call<F>));
		stor_ptr = new(&stor) wait_fn_call<F>(f);
	}

	inline void reset(void* p = nullptr) { stor_ptr = p; }
	inline bool operator()() { return (*reinterpret_cast<wait_fn_call_base*>(stor_ptr))(); }
	inline operator void*() { return stor_ptr; }

private:
	void* stor_ptr;
	wait_fn_stor<S> stor;
};

/* ------------------------------------------------------------------------- */

template<size_t S>
struct __attribute__((packed)) user_stack
{
	user_stack(void *rip, void(*ret)())
		: state {
#ifdef TASK_SWITCH_SAVE_CALLER_SAVED
			0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
				0, 0, 0, 0, 0, 0, (uint64_t) rip, (uint64_t) ret }
	{   
		memset(space, 0, sizeof(space));
	}

	template<typename T>
	inline T top() { return (T) (((uint64_t) this) + S); }

private:
	uint8_t space[S - sizeof(switch_regs) - sizeof(uint64_t)];

public:
	switch_regs state;
};

/* ------------------------------------------------------------------------- */

struct task
{
	task(int id, uint64_t rsp, uint64_t tss_rsp)
		: id(id), rsp(rsp), tss_rsp(tss_rsp), running(false)
	{ }

	int id;

	uint64_t rsp;       /* Saved stack pointer */
	uint64_t tss_rsp;   /* Kernel stack top */

	wait_fn<> wait_for;

	inline bool can_run()
	{
		if (running)
		{
			if (wait_for)
				return wait_for();
			return true;
		}
		return false;
	}

	bool running;

	task* next;
};

#endif /* TASK_H */
