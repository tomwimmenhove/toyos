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

#if 1
void switch_to(interrupt_state* state)
{
	asm volatile(
			"mov $0x3b, %%ax\n"
			"mov %%ax, %%ds\n"
			"mov %%ax, %%es\n"
			"mov %%ax, %%fs\n"
			"mov %%ax, %%gs\n"

			"mov %0, %%rsp\n"
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
			: "r" (state)
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
	con << ".";

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
	con << "key: " << (long long int) ch << "\n";

	if (ch > 127)
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

