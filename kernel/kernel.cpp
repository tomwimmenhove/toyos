#include <stdint.h>
#include <stddef.h>

#include "config.h"
#include "linker.h"
#include "new.h"
#include "mb.h"
#include "memory.h"
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

extern "C" void __cxa_pure_virtual()
{
	panic("Virtual method called");
}

extern "C" void *memset(void *s, int c, size_t n)
{
	uint8_t* p = (uint8_t*) s;

	while (n--)
		*p++ = c;

	return s;
}

void print_stack_use()
{
	con << "stack usuage: " << (KERNEL_STACK_TOP - (uint64_t) __builtin_frame_address(0)) << "\n";
}

void __attribute__ ((noinline)) test()
{
		asm("int $42");
}

void user_space()
{
	for (int i = 0;; i ++)
	{
//		for (volatile int j = 0; j < 10000000; j++) ;
		ucon << '1';
//		ucon << "Hello world from userspace process 1: i=" << i << '\n';
//		for (int i = 0 ; i < 9; i++)
			syscall(2);
//		syscall(3);
	}
}

void user_space2()
{
	for (int i = 0;; i ++)
	{
//		for (volatile int j = 0; j < 10000000; j++) ;
		ucon << '2';
//		ucon << "Hello world from userspace process 2: i=" << i << '\n';
//		for (int i = 0 ; i < 9; i++)
			syscall(2);
//		syscall(4);
	}
}

void dead_task()
{
	panic("Can't handle returning tasks yet");
}

void uspace_test()
{
	for (;;)
	{
		ucon << '1';
	}
}

template<size_t S>
struct __attribute__((packed)) user_stack
{
	user_stack(void *rip, void(*ret)())
		: lsp(sizeof(space)), state {
#ifdef TASK_SWITCH_SAVE_CALLER_SAVED
			0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
			0, 0, 0, 0, 0, 0, (uint64_t) rip, (uint64_t) ret }
	{   
		memset(space, 0, sizeof(space));
	}

	template<typename T>
		inline T top() { return (T) (((uint64_t) this) + S); }

	template<typename T>
		inline void push(T x)
		{   
			lsp -= sizeof(T);
			void* p = &space[lsp];
			*(T*)p = x;
		}

	template<typename T>
		inline T pop()
		{   
			void* p = &space[lsp];
			lsp += sizeof(T);
			return *(T*) p;
		};

	private:
	uint64_t lsp;
	uint8_t space[S - sizeof(switch_regs) - sizeof(uint64_t)];

	public:
	switch_regs state;
};

user_stack<PAGE_SIZE>* ustack1;
user_stack<PAGE_SIZE>* ustack2;

struct task
{
	task(int id, uint64_t rsp, uint64_t tss_rsp)
		: id(id), rsp(rsp), tss_rsp(tss_rsp), running(false)
	{ }

	int id;

	uint64_t rsp;		/* Saved stack pointer */
	uint64_t tss_rsp;	/* Kernel stack top */

	bool running;

	task* next;
};

task* tasks = nullptr;
task* current = nullptr;

task* task_idle;
task* task1;
task* task2;

void schedule();

extern "C" void k_test_user1(uint64_t arg0)
{
	ucon << "arg0: " << hex_u64(arg0) << '\n';

	for (;;)
	{
		ucon << '1';
		task1->running = false;
		syscall(5);
	
//		syscall(2);
	}
}

void k_test_user2()
{
	for (;;)
	{
		ucon << '2';
		task2->running = false;
		syscall(5);
//		syscall(2);
	}
}

void k_test2()
{
	jump_uspace((uint64_t) &k_test_user2, ustack2->top<uint64_t>());
}



void task_add(task* task)
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

		/* Running task? */
		if (current->running)
			break;

		/* Nothing to do? */
		if (current == last)
			current = task_idle;
	}

	/* Setup new kernel stack */
	tss0.rsp0 = current->tss_rsp;

	/* Switch tasks */
	state_switch(current->rsp, last->rsp);
}

void tsk_idle()
{
	for (;;)
	{
//		ucon << 'X';
		asm("hlt");
//		schedule();
	}
}

void k_test_init()
{
	/* Use the top of our current stack. It's safe to smash it, since we won't return.
	 * We'll use 0 for the tss_rsp, because the idle task is never in user space and
	 * will never cause a privilege change. */
	task_idle = new task(0, KERNEL_STACK_TOP, 0);
	task_add(task_idle);

	/* Here we set up our stack for the task. One initial page. The rip entry is set
	 * to the entry point of the function that will, in turn, call user space */
	ustack1 = new user_stack<PAGE_SIZE>((void*) &uspace_jump_trampoline, &dead_task);
	ustack2 = new user_stack<PAGE_SIZE>((void*) &uspace_jump_trampoline, &dead_task);

#ifdef TASK_SWITCH_SAVE_CALLER_SAVED
	ustack1->state.rdi = (uint64_t) &k_test_user1;
	ustack1->state.rsi = ustack1->top<uint64_t>();

	ustack2->state.rdi = (uint64_t) &k_test_user2;
	ustack2->state.rsi = ustack2->top<uint64_t>();
#else
	ustack1->state.r12 = (uint64_t) &k_test_user1;
	ustack1->state.r13 = ustack1->top<uint64_t>();

	ustack2->state.r12 = (uint64_t) &k_test_user2;
	ustack2->state.r13 = ustack2->top<uint64_t>();
#endif
	ustack1->push<uint64_t>(0xaaaaaaaabbbbbbbb);


	/* Set up the task. Use the registers state (rip and return pointer, as set above) as
	 * stack pointer. This way, when the scheduler comes along and performs the switch to
	 * this task, it will neatly pop those registers, and 'return' to the function
	 * pointed to by state->rip. After which that function performs the actual jump
	 * to userspace. */
	task1 = new task(1, (uint64_t) &ustack1->state, (uint64_t) new uint8_t[PAGE_SIZE] + PAGE_SIZE);
	task2 = new task(2, (uint64_t) &ustack2->state, (uint64_t) new uint8_t[PAGE_SIZE] + PAGE_SIZE);


	task_add(task1);
	task_add(task2);

	task_idle->running = true;
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

void intr_syscall(uint64_t, interrupt_state* state)
{
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
	}
}

void interrupt_timer(uint64_t, interrupt_state*)
{
//	uint64_t test_rsp = 0;

//	con << ".";
//	state_switch(rsp2, &rsp1);
	schedule();
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);
//	con << "key: " << (long long int) ch << "\n";

	if (ch > 127)
		return;

	if (ch == 2)
		task1->running = true;
	if (ch == 3)
		task2->running = true;

	schedule();
}

void kmain()
{
	mallocator::test();

//	pic_sys.disable(pic_sys.to_intr(0));

	interrupts::regist(pic_sys.to_intr(0), interrupt_timer);
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
	interrupts::regist(42, intr_syscall);


//	test();

	/* Load TSS0 */
	asm volatile("mov $0x20, %%ax\n"
	"ltr %%ax"
	::: "%ax");

	uint64_t ps = (uint64_t) mallocator::malloc(PAGE_SIZE);
	uint64_t ps_top = ps + PAGE_SIZE;

	tss0.rsp0 = ps_top;
//	pic_sys.sti();
	k_test_init();
}

extern "C"
{
	void _init();
	void _fini();

	void _start(kernel_boot_info* kbi);
}

void _start(kernel_boot_info* kbi)
{
	_init();

	if (kbi->magic != KBI_MAGIC)
		panic("Bad magic number!");

	con.init(kbi);
	con << "\n";

	gdt_init();
	idt_init();

	memory::init(kbi);

	/* Map vide text buffer into memory */
	uint64_t video_base = 0xffffffff40000000 - 0x1000;
	memory::map_page(video_base, 0xb8000);
	con.base = (uint8_t*) video_base;

	con << "Video ram remapped\n";

	/* Unmap unused memmory */
	memory::unmap_unused();


	mallocator::init(0xffffa00000000000, 1024 * 1024 * 1024);

	kmain();
	_fini();

	panic("kmain() returned!?");
}

