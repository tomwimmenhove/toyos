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

template<size_t S>
struct __attribute__((packed)) kstack
{
	kstack(uint64_t rip, uint64_t rsp, uint16_t cs = 0x33, uint64_t rflags = 0x202, uint16_t ss = 0x3b)
		:state {
			/* rsp */ ((uint64_t) &state) + sizeof(uint64_t), /* Because, after popping %rsp itself, it should point to the next register to be popped */
			/* GP regs */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			{ rip, cs, rflags, rsp, ss /* iregs */} } 
	{
		memset(space, 0, sizeof(space));
	}

	template<typename T>
	inline T top() { return (T) (((uint64_t) this) + S); }
	uint8_t space[S - sizeof(interrupt_state)];
	interrupt_state state;
};

static kstack<4096>* kstack1;
static kstack<4096>* kstack2;

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


struct __attribute__((packed)) regs
{
	/* XXX: Wrap caller-saved registers in DEBUG ifdefs. */
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;

	uint64_t rip;

	/* Not really a register, but very handy to
	 * be able to set a return address. */
	uint64_t ret_ptr;
};

void dead_task()
{
	panic("Can't handle returning tasks yet");
}

/* Used to start the very first task. */
void __attribute__ ((noinline)) init_tsk0(uint64_t rip, uint64_t ret, uint64_t rsp)
{
	asm volatile(
			"mov %%rdx, %%rsp\n"	// Set stack pointer
			"push %%rsi\n"			// Push return pointer
			"jmp *%%rdi\n"			// Jump to task
			: 
			: "d" (rsp), "S" (ret), "D" (rip));
}

void __attribute__ ((noinline)) jump_uspace(uint64_t rip, uint64_t rsp, uint16_t cs = 0x33, uint64_t rflags = 0x202, uint16_t ss = 0x3b)
{
	asm volatile(
			"mov %0, %%rax\n"	// Setup data segment
			"mov %%ax, %%ds\n"
			"mov %%ax, %%es\n"
			"mov %%ax, %%fs\n"
			"mov %%ax, %%gs\n"

			"pushq %0\n"		// push ss
			"pushq %1\n"		// Push rsp
			"pushq %2\n"		// Push flags
			"pushq %3\n"		// Push cs
			"pushq %4\n"		// Push rip

			"iretq\n"			// Jump to userspace!
			: 
			: "r" ((uint64_t) ss), "r" (rsp), "r" (rflags), "r" ((uint64_t) cs), "r" ((uint64_t) rip)
			: "memory"
			);
}

void uspace_test()
{
	for (;;)
	{
		ucon << '1';
	}
}

/* Perform a state switch. Current state will be saved on the current stack frame before
 * saving the stack pointer in save_rsp. Then, the new stack pointer will be loaded from
 * rsp before finally restoring the state from the new stack frame.
 * NOTE: If, for any reason, this function could be called to switch between identical stack
 * frames, make sure that (&rsp == &save_rsp). I.e. if the scheduler schedules the same task
 * twice in s row, use the actual rsp entry in the task structure as reference for both
 * arguments, and don't use a copy of rsp! */
void __attribute__ ((noinline)) state_switch(uint64_t& rsp, uint64_t& save_rsp)
{   
	uint64_t rsp_ptr = (uint64_t) &rsp;
	uint64_t save_rsp_ptr = (uint64_t) &save_rsp;

	asm volatile(
			/* XXX: Wrap caller-saved registers in DEBUG ifdefs. */
			/* Save registers of the current task */
			"pushq %%rax\n"
			"pushq %%rcx\n"
			"pushq %%rdx\n"
			"pushq %%rbx\n"
			"pushq %%rbp\n"
			"pushq %%rsi\n"
			"pushq %%rdi\n"
			"pushq %%r8\n"
			"pushq %%r9\n"
			"pushq %%r10\n"
			"pushq %%r11\n"
			"pushq %%r12\n"
			"pushq %%r13\n"
			"pushq %%r14\n"
			"pushq %%r15\n"

			/* Memory address of the stack pointer to save is in %rsi */
			"mov %%rsp, (%%rsi)\n"

			/* New stack pointer is in %rdi */
			"mov (%%rdi), %%rsp\n"

			/* Restore registers of the next task */
			"popq %%r15\n"
			"popq %%r14\n"
			"popq %%r13\n"
			"popq %%r12\n"
			"popq %%r11\n"
			"popq %%r10\n"
			"popq %%r9\n"
			"popq %%r8\n"
			"popq %%rdi\n"
			"popq %%rsi\n"
			"popq %%rbp\n"
			"popq %%rbx\n"
			"popq %%rdx\n"
			"popq %%rcx\n"
			"popq %%rax\n"
			: : "S" (save_rsp_ptr), "D" (rsp_ptr)
			: "memory");
}

template<size_t S>
struct __attribute__((packed)) user_stack
{
	user_stack(void (*rip)(), void(*ret)())
		: state { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (uint64_t) rip, (uint64_t) ret }
	{
		memset(space, 0, sizeof(space));
	}

	template<typename T>
	inline T top() { return (T) (((uint64_t) this) + S); }
	uint8_t space[S - sizeof(regs)];
	regs state;
};

user_stack<4096>* ustack1;
user_stack<4096>* ustack2;

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
void k_test_user1()
{
	for (;;)
	{
		ucon << '1';
		task1->running = false;
		syscall(5);
	
//		syscall(2);
	}
}

void k_test1()
{
	jump_uspace((uint64_t) &k_test_user1, ustack1->top<uint64_t>());
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
	ustack1 = new user_stack<4096>(&k_test1, &dead_task);
	ustack2 = new user_stack<4096>(&k_test2, &dead_task);

	/* Set up the task. Use the registers state (rip and return pointer, as set above) as
	 * stack pointer. This way, when the scheduler comes along and performs the switch to
	 * this task, it will neatly pop those registers, and 'return' to the function
	 * pointed to by state->rip. After which that function performs the actual jump
	 * to userspace. */
	task1 = new task(1, (uint64_t) &ustack1->state, (uint64_t) new uint8_t[4096] + 4096);
	task2 = new task(2, (uint64_t) &ustack2->state, (uint64_t) new uint8_t[4096] + 4096);

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


#if 1
void switch_to(interrupt_state* state)
{
//	uint64_t b = (uint64_t) &state;
	asm volatile(
			"mov $0x3b, %%ax\n"
			"mov %%ax, %%ds\n"
			"mov %%ax, %%es\n"
			"mov %%ax, %%fs\n"
			"mov %%ax, %%gs\n"

			"mov (%0), %%rsp\n"
			"popq %%rsp\n"
			"popq %%rax\n"
			"popq %%rcx\n"
			"popq %%rdx\n"
			"popq %%rbx\n"
			"popq %%rbp\n"
			"popq %%r8\n"
			"popq %%r9\n"
			"popq %%r10\n"
			"popq %%r11\n"
			"popq %%r12\n"
			"popq %%r13\n"
			"popq %%r14\n"
			"popq %%r15\n"
			"pop %%rsi\n"
			"pop %%rdi\n"
			"add $8, %%rsp\n"
			"iretq\n"
			:
//			: "r" (b)
			: "r" (&state)
			: "memory", "%rax"
			);
}
#endif

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
#if 0
		case 3: // load process 2
			tss0.rsp0 = kstack2.top<uint64_t>();
			
			tmp = state_p2->rsp;
			state_p2->rsp = state_p1->rsp;
			state_p1->rsp = tmp;

//			switch_to(state_p2);

			break;
		case 4: // load process 1
			tss0.rsp0 = kstack1.top<uint64_t>();

			tmp = state_p1->rsp;
			state_p1->rsp = state_p2->rsp;
			state_p2->rsp = tmp;

//			switch_to(state_p1);
#endif
	}
}

void interrupt_timer(uint64_t, interrupt_state*)
{
//	uint64_t test_rsp = 0;

//	con << ".";
//	state_switch(rsp2, &rsp1);
	schedule();
	return;

	if (tss0.rsp0 == kstack2->top<uint64_t>())
	{
		tss0.rsp0 = kstack1->top<uint64_t>();
		switch_to(&kstack1->state);
	}
	else
	{
		tss0.rsp0 = kstack2->top<uint64_t>();
		switch_to(&kstack2->state);
	}
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
	return;
	if (tss0.rsp0 == kstack2->top<uint64_t>())
	{
		tss0.rsp0 = kstack1->top<uint64_t>();
		switch_to(&kstack1->state);
	}
	else
	{
		tss0.rsp0 = kstack2->top<uint64_t>();
		switch_to(&kstack2->state);
	}
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

	uint64_t ps = (uint64_t) mallocator::malloc(4096);
	uint64_t ps_top = ps + 4096;

	tss0.rsp0 = ps_top;
//	pic_sys.sti();
	k_test_init();


	uint64_t ustack_p1 = (uint64_t) mallocator::malloc(4096);
	uint64_t ustack_p1_top = (uint64_t) ustack_p1 + 4096;

	uint64_t ustack_p2 = (uint64_t) mallocator::malloc(4096);
	uint64_t ustack_p2_top = ustack_p2 + 4096;

	/* Setup processes */
	kstack1 = new kstack<4096>((uint64_t) &user_space, ustack_p1_top);
	kstack2 = new kstack<4096>((uint64_t) &user_space2, ustack_p2_top);

	/* Enable interrupts */
	pic_sys.sti();

	/* Switch to the first process */
	tss0.rsp0 = kstack1->top<uint64_t>();
	switch_to(&kstack1->state);
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

