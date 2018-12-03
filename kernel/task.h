#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <vector>
#include <embxx/util/StaticFunction.h>

#include "klib.h"
#include "task_helper.h"
#include "dev.h"
#include "new.h"
#include "mem.h"

void schedule();

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
		: id(id), rsp(rsp), tss_rsp(tss_rsp), cr3(cr3_get())
	{ }

	task(int id, std::unique_ptr<task_stack> tsk_stack, std::unique_ptr<uint8_t[]> k_stack, size_t k_stack_size)
		: id(id),
		  rsp((uint64_t) tsk_stack->init_regs), /* rsp at bottom of init_regs, ready to pop. */
		  tss_rsp((uint64_t) k_stack.get() + k_stack_size), /* tss_rsp at top of kernel stack. */
		  cr3(memory::clone_tables()),
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
	
	embxx::util::StaticFunction<bool()> wait_for;
	
	std::vector<std::shared_ptr<io_handle>> io_handles;
	std::forward_list<mapped_io_handle> mapped_io_handles;

	std::shared_ptr<task> next;

	uint64_t rsp;       /* Task's current stack pointer */
	uint64_t tss_rsp;   /* Kernel stack top */

	uint64_t cr3;

private:
	std::unique_ptr<task_stack> tsk_stack;
	std::unique_ptr<uint8_t[]> k_stack;
};

#endif /* TASK_H */
