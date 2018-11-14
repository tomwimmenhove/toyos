#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>
#include <memory>

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

struct task_stack
{
	task_stack(size_t size, uint64_t rip,  uint64_t ret)
		: size(size), space(std::make_unique<uint8_t[]>(size))
	{
		memset(space.get(), 0, sizeof(space));

		/* Place the registers at the top of the stack */
		init_regs = new(&space[size - sizeof(switch_regs)]) switch_regs();

		/* Set values */
		init_regs->rip = rip;
		init_regs->ret_ptr = ret;
	}

	template<typename T>
	inline T top() { return (T) (((uint64_t) space.get()) + size); }

	switch_regs* init_regs;

private:
	size_t size;
	std::unique_ptr<uint8_t[]> space;
};

/* ------------------------------------------------------------------------- */

struct task
{
	task(int id, uint64_t rsp, uint64_t tss_rsp)
		: id(id), rsp(rsp), tss_rsp(tss_rsp)
	{ }

	task(int id, std::unique_ptr<task_stack> tsk_stack, std::unique_ptr<uint8_t[]> k_stack, size_t k_stack_size)
		: id(id),
		  rsp((uint64_t) tsk_stack->init_regs), /* rsp at bottom of init_regs, ready to pop. */
		  tss_rsp((uint64_t) k_stack.get() + k_stack_size), /* tss_rsp at top of kernel stack. */
		  tsk_stack(std::move(tsk_stack)),
		  k_stack(std::move(k_stack))
	{ }

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

	int id;
	bool running = false;
	
	wait_fn<> wait_for;
	
	std::shared_ptr<task> next;

	uint64_t rsp;       /* Task's current stack pointer */
	uint64_t tss_rsp;   /* Kernel stack top */

private:
	std::unique_ptr<task_stack> tsk_stack;
	std::unique_ptr<uint8_t[]> k_stack;
};

#endif /* TASK_H */
