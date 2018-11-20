#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <embxx/container/StaticQueue.h>
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
#include "dev.h" 
#include "syscalls.h"

void print_stack_use()
{
	con << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void dead_task()
{
	panic("Can't handle returning tasks yet");
}

uint64_t jiffies = 0;

void schedule();

std::shared_ptr<task> tasks;
std::shared_ptr<task> current;

std::shared_ptr<task> task_idle;

embxx::container::StaticQueue<uint8_t, 4096> key_queue;

struct driver_kbd : public driver_handle
{
	size_t read(void* buf, size_t len) override
	{
		current->wait_for = []() { return key_queue.size() != 0; };

		uint8_t* b = (uint8_t*) buf;

		size_t t = 0;
		while (len)
		{
			/* Wait until data is available */
			if (!key_queue.size())
					schedule();

			/* XXX: LOCK SHIT HERE */
			b[t] = key_queue.back();
			key_queue.pop_back();

			t++;
			len--;
		}

		/* No longer waiting */
		current->wait_for = nullptr;

		return t;
	}

	size_t write(void* buf, size_t len) override
	{
		con.write_buf((const char*) buf, len);

		return len;
	}

	bool open(int) override { return true; }
	bool close(int) override { return true; }
};

devices devs;

#include "cache_alloc.h"

struct tst
{
	tst(int i) : i(i) { con << 'c' << ((uint64_t) this) << '\n'; }
	~tst() { con << 'd' << ((uint64_t) this) << '\n'; }

	int i;
};

void test()
{
	cache_alloc<tst> test;

	auto a = test.take(42);
	auto b = test.take(34);
	auto c = test.take(45);
	test.release(b);
	auto d = test.take(6);

	con << "{\n";
	{
		con << "{\n";
		auto e = test.take_shared(13);
		con << "e: " << ((uint64_t) e.get()) << " = " << e->i << '\n';
		e = test.take_shared(13);
		con << "e: " << ((uint64_t) e.get()) << " = " << e->i << '\n';
		con << "}\n";
	}
	con << "}\n";

	con << "{\n";
	{
		con << "{\n";
		auto e = test.take_shared(15);
		con << "e: " << ((uint64_t) e.get()) << " = " << e->i << '\n';
		con << "}\n";
	}
	con << "}\n";

	con << "a: " << a->i << '\n';
	con << ((uint64_t) a) << '\n';
	con << ((uint64_t) b) << '\n';
	con << ((uint64_t) c) << '\n';
	con << ((uint64_t) d) << '\n';

	auto drv_kbd = std::make_shared<driver_kbd>();

	drv_kbd->dev_type = 0;

	devs.add(drv_kbd);
}

extern "C" void k_test_user1(uint64_t arg0, uint64_t arg1)
{
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 1: arg0: " << arg0 << '\n';
	ucon << "tsk 1: arg1: " << arg1 << '\n';

	for (;;)
	{
		ucon << "1";
		syscall(7);
	}
}

void k_test_user2(uint64_t arg0, uint64_t arg1)
{
	int fd = open(0, 0);
	console_user ucon(fd);

    ucon << "tsk 2: arg0: " << arg0 << '\n';
	ucon << "tsk 2: arg1: " << arg1 << '\n';

	uint8_t buf[2];
	for (;;)
	{
		/* Read syscall:   sysc  fd  buffer          size */
		auto len = read(fd, (void*) buf, sizeof(buf));

		for (size_t i = 0; i < len; i++)
			ucon << '(' << buf[i] << ')';
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

static inline void halt()
{
	asm volatile(
			"sti\n"
			"hlt\n"
			"cli\n");
}

void tsk_idle()
{
	for (;;)
	{
		schedule();
//		ucon << 'X';
		halt();
	}
}

void k_test_init()
{
	test();

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

	/* Set up the task. Use the registers state (rip and return pointer, as set above) as
	 * stack pointer. This way, when the scheduler comes along and performs the switch to
	 * this task, it will neatly pop those registers, and 'return' to the function
	 * pointed to by state->rip. After which that function performs the actual jump
	 * to userspace. */
	auto task1 = std::make_shared<task>(1, std::move(ustack1), std::make_unique<uint8_t[]>(KSTACK_SIZE), KSTACK_SIZE);
	auto task2 = std::make_shared<task>(2, std::move(ustack2), std::make_unique<uint8_t[]>(KSTACK_SIZE), KSTACK_SIZE);

	task_add(task1);
	task_add(task2);

	task1->running = true;
	task2->running = true;

	tss0.rsp0 = 0;
	current = task_idle;

	/* Enable global interrupts */
	pic_sys.sti();

	/* Start the idle task. Since we will never return from this, we can safely set the
	 * stack pointer to the top of our current stack frame. */
	init_tsk0((uint64_t) tsk_idle, (uint64_t) &dead_task, KERNEL_STACK_TOP);

	panic("Idle task returned!?");
}

int kopen(int arg0, int arg1)
{
	auto handle = devs.open(arg0, arg1);
	if (!handle)
		return -1;

	int fd = -1;
	/* XXX: LOCK SHIT */
	for (size_t i = 0; i < current->dev_handles.size(); i++)
	{
		if (!current->dev_handles[i])
		{
			fd = i;
			break;
		}
	}

	if (fd == -1)
	{
		fd = current->dev_handles.size();
		/* Push a new one */
		current->dev_handles.push_back(handle);
	}
	else
		current->dev_handles[fd] = handle;
	/* XXX: Safe. Unlock */

	return (size_t) fd;;
}

size_t kread(int fd, void *buf, size_t len)
{
	if ((size_t) fd >= current->dev_handles.size())
		return -1;

	auto handle = current->dev_handles[fd];

	return handle->read(buf, len);
}

size_t kwrite(int fd, void *buf, size_t len)
{
	if ((size_t) fd >= current->dev_handles.size() || !current->dev_handles[fd])
		return -1;

	auto handle = current->dev_handles[fd];

	return handle->write(buf, len);
}

extern "C" size_t syscall_handler(uint64_t syscall_no, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	auto now = jiffies;
	(void) arg0, (void) arg1, (void) arg2, (void) arg3, (void) arg4;

	switch(syscall_no)
	{
		case 0:
			{
				const unsigned char* s = (const unsigned char*) arg0;
				auto len = arg1;
				for (size_t i = 0; i < len; i++)
					con.putc(*s++);
				break;
			}
		case 1:
			con.putc(arg0);
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

		case 0x10:
			return (size_t) kopen(arg0, arg1);
		case 0x13:
			return (size_t) kread(arg0, (void*) arg1, arg2);
		case 0x14:
			return (size_t) kwrite(arg0, (void*) arg1, arg2);
		default:
			panic("Invalid system call");
	}

	return -1;
}

void interrupt_timer(uint64_t, interrupt_state*)
{
	jiffies++;
//	con << '.';
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);

	key_queue.push_back(ch);
}

void kmain()
{
	mallocator::test();

//	pic_sys.disable(pic_sys.to_intr(0));

	interrupts::init();
	interrupts::regist(pic_sys.to_intr(0), interrupt_timer, true);
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
//	interrupts::regist(42, intr_syscall);

//	pic_sys.sti();
	k_test_init();
}
