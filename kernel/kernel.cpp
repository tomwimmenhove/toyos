#include <stdint.h>
#include <stddef.h>
#include <memory>

#include "config.h"
#include "linker.h"
#include "new.h"
#include "mb.h"
#include "mem.h"
#include "malloc.h"
#include "frame_alloc.h"
#include "descriptors.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "pic.h"
#include "console.h"
#include "syscall.h"
#include "task_helper.h"
#include "task.h"
#include "klib.h"

void print_stack_use()
{
	con << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void dead_task()
{
	panic("Can't handle returning tasks yet");
}

uint64_t jiffies = 0;


std::shared_ptr<task> tasks;
std::shared_ptr<task> current;

std::shared_ptr<task> task_idle;
std::shared_ptr<task> task1;
std::shared_ptr<task> task2;

void schedule();

extern "C" void k_test_user1(uint64_t arg0, uint64_t arg1)
{
	ucon << "tsk 1: arg0: " << arg0 << '\n';
	ucon << "tsk 1: arg1: " << arg1 << '\n';

	for (;;)
	{
		ucon << '1';
		syscall(7);
	}
}

static uint8_t last_code = 0;

void k_test_user2(uint64_t arg0, uint64_t arg1)
{
	ucon << "tsk 2: arg0: " << arg0 << '\n';
	ucon << "tsk 2: arg1: " << arg1 << '\n';

	for (;;)
	{
		syscall(8);
		ucon << '(' << last_code << ')';
	}
}

void task_add(std::shared_ptr<task> task)
{
	auto head = tasks;

	tasks = task;
	task->next = head;
}

void schedule()
{
	auto last = current;

	for(;;)
	{
		current = current->next;
		if (current == nullptr)
			current = tasks;

		if (current->can_run())
			break;

		if (current == last)
		{
			/* Nothing to do? */
			if (!current->can_run())
				current = task_idle;
			break;
		}
	}

//	con << "Switched to task " << current->id << '\n';

	/* Setup new kernel stack */
	tss0.rsp0 = current->tss_rsp;

	/* Switch tasks */
	state_switch(current->rsp, last->rsp);
}

/* Sets up a temporary new stack for incoming interrupts to use */
void save_kernel_halt()
{
	static uint8_t stack[KSTACK_SIZE];

	asm volatile("mov %%rsp, %%rax\n"
			"mov %0, %%rsp\n"
			"sti\n"
			"hlt\n"
			"cli\n"
			"mov %%rax, %%rsp\n"
			: : "r" (stack + KSTACK_SIZE)
			: "memory", "%rax");
}

void tsk_idle()
{
	for (;;)
	{
		save_kernel_halt();
//		ucon << 'X';
		schedule();
	}
}

void k_test_init()
{
	/* Use the top of our current stack. It's safe to smash it, since we won't return.
	 * We'll use 0 for the tss_rsp, because the idle task is never in user space and
	 * will never cause a privilege change. */
	task_idle = std::make_shared<task>(0, KERNEL_STACK_TOP, 0);

	task_add(task_idle);

	/* Here we set up our stack for the task. One initial page. The rip entry is set
	 * to the entry point of the function that will, in turn, call user space */
	auto ustack1 = std::make_unique<task_stack>(PAGE_SIZE, (uint64_t) &uspace_jump_trampoline, (uint64_t) &dead_task);
	auto ustack2 = std::make_unique<task_stack>(PAGE_SIZE, (uint64_t) &uspace_jump_trampoline, (uint64_t) &dead_task);

#ifdef TASK_SWITCH_SAVE_CALLER_SAVED
	ustack1->init_regs.rdi = (uint64_t) &k_test_user1;
	ustack1->init_regs.rsi = ustack1->top<uint64_t>();

	ustack2->init_regs.rdi = (uint64_t) &k_test_user2;
	ustack2->init_regs.rsi = ustack2->top<uint64_t>();
#else
	ustack1->init_regs->r12 = (uint64_t) &k_test_user1;
	ustack1->init_regs->r13 = ustack1->top<uint64_t>();
	ustack1->init_regs->r14 = 42;	// Will be passed as arguments
	ustack1->init_regs->r15 = 43;

	ustack2->init_regs->r12 = (uint64_t) &k_test_user2;
	ustack2->init_regs->r13 = ustack2->top<uint64_t>();
	ustack2->init_regs->r14 = 44;
	ustack2->init_regs->r15 = 45;
#endif

	con << "Creating tasks\n";


	/* Set up the task. Use the registers state (rip and return pointer, as set above) as
	 * stack pointer. This way, when the scheduler comes along and performs the switch to
	 * this task, it will neatly pop those registers, and 'return' to the function
	 * pointed to by state->rip. After which that function performs the actual jump
	 * to userspace. */
	task1 = std::make_shared<task>(1, std::move(ustack1), std::make_unique<uint8_t[]>(KSTACK_SIZE), KSTACK_SIZE);
	task2 = std::make_shared<task>(2, std::move(ustack2), std::make_unique<uint8_t[]>(KSTACK_SIZE), KSTACK_SIZE);

	con << "Created tasks\n";

	task_add(task1);
	task_add(task2);

//	task_idle->running = true;
	task1->running = true;
	task2->running = true;

	tss0.rsp0 = 0;
	current = task_idle;

	/* Enable global interrupts */
	pic_sys.sti();

	con << "BOOM\n";

	/* Start the idle task. Since we will never return from this, we can safely set the
	 * stack pointer to the top of our current stack frame. */
	init_tsk0((uint64_t) tsk_idle, (uint64_t) &dead_task, KERNEL_STACK_TOP);

	panic("Idle task returned!?");
}

#if 0
static bool wait_jiffies()
{
	return (jiffies - now) >= 18;
//	return (jiffies - now) > 0;
}

static bool wait_key()
{
	return last_code != 0;
}
#endif

void intr_syscall(uint64_t, interrupt_state* state)
{
	auto now = jiffies;


	switch(state->rdi)
	{
		case 0:
			con.write_string((const char*) state->rsi);
			break;
		case 1:
			con.putc(state->rsi);
			break;
		case 2:
			asm volatile(
					"sti\n"
					"hlt");
			break;

		case 5:
			schedule();
			break;

		case 6:
			current->running = false;
			schedule();
			break;

		case 7:
			current->wait_for = [now]() { return (jiffies - now) >= 18; };
			schedule();
			current->wait_for = nullptr;
			break;

		case 8:
			last_code = 0;
			current->wait_for = []() { return last_code != 0; };
			schedule();
			current->wait_for = nullptr;
			break;
	}
}

void interrupt_timer(uint64_t, interrupt_state*)
{
	jiffies++;

	if (jiffies % 5 == 0)
		schedule();
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);
//	con << "key: " << (long long int) ch << "\n";

	if (ch > 127)
		return;

	last_code = ch;

	if (ch == 2)
		task1->running = true;
	if (ch == 3)
		task2->running = true;

//	schedule();
}

#include <embxx/util/StaticFunction.h>

void kmain()
{
//	die();

	mallocator::test();

//	pic_sys.disable(pic_sys.to_intr(0));

	interrupts::regist(pic_sys.to_intr(0), interrupt_timer);
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
	interrupts::regist(42, intr_syscall);

//	test();

//	pic_sys.sti();
	k_test_init();
}
