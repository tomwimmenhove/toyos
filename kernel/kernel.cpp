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
	template<typename T>
	inline T top() { return (T) (((uint64_t) this) + S); }
	uint8_t space[S - sizeof(interrupt_state)];
	interrupt_state state;
};

static kstack<4096> kstack1;
static kstack<4096> kstack2;

static uint64_t ustack2;

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

struct task
{
	uint64_t krsp;
	uint64_t rsp;
};

uint8_t b2[4096];
uint8_t b4[4096];


interrupt_state* state_p1;

uint64_t ustack_p1 = (uint64_t) b2;
uint64_t ustack_p1_top = ustack_p1 + sizeof(b2);;

interrupt_state* state_p2;

uint64_t ustack_p2 = (uint64_t) b4;
uint64_t ustack_p2_top = ustack_p2 + sizeof(b4);



void intr_syscall(uint64_t, interrupt_state* state)
{
//	con << "enter\n";
	//	tss0.rsp0 -= 0x100;

	uint64_t tmp;
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
	}
}

void interrupt_timer(uint64_t, interrupt_state*)
{
	con << ".";

//	return;
	if (tss0.rsp0 == kstack2.top<uint64_t>())
	{
		tss0.rsp0 = kstack1.top<uint64_t>();
		switch_to(state_p1);
	}
	else
	{
		tss0.rsp0 = kstack2.top<uint64_t>();
		switch_to(state_p2);
	}
}

void interrupt_kb(uint64_t, interrupt_state*)
{
	uint8_t ch = inb_p(0x60);
	con << "key: " << (long long int) ch << "\n";

	if (ch > 127)
		return;

//	return;
	if (tss0.rsp0 == kstack2.top<uint64_t>())
	{
		tss0.rsp0 = kstack1.top<uint64_t>();
		switch_to(state_p1);
		con << "Never happens!\n";
	}
	else
	{
		tss0.rsp0 = kstack2.top<uint64_t>();
		switch_to(state_p2);
		con << "Never happens!\n";
	}
}

struct newTest
{
	char s[8192];
	int a;
};

extern "C" void *memset(void *s, int c, size_t n)
{
	uint8_t* p = (uint8_t*) s;

	while (n--)
		*p++ = c;

	return s;
}


void kmain()
{
	mallocator::test();

#if 0
	// XXX: Just a test!
	uint8_t* ppp = (uint8_t*) 0xffffa00000000000;
	for (int i = 0; i < 126 * 1024 * 1024; i += 0x1000)
	{ 
		mem.map_page((uint64_t) ppp, mem.frame_alloc->page());
		ppp += 0x1000;
	} 
	uint64_t* pppp = (uint64_t*) 0xffffa00000000000;
	for (uint64_t i = 0; i < 126 * 1024 * 1024 / sizeof(uint64_t); i++)
		pppp[i] = 0x41414141;
#endif


//	pic_sys.disable(pic_sys.to_intr(0));

	interrupts::regist(pic_sys.to_intr(0), interrupt_timer);
	interrupts::regist(pic_sys.to_intr(1), interrupt_kb);
	interrupts::regist(42, intr_syscall);

	volatile newTest* nt = new newTest();

	nt->a = 42;

	con << "test: " << nt->a << '\n';

	

//	test();

	/* Load TSS0 */
	asm volatile("mov $0x20, %%ax\n"
	"ltr %%ax"
	::: "%ax");

	/* SETUP PS 1 */
	tss0.rsp0 = kstack1.top<uint64_t>();

	/* Place state at the top of the process' stack */
	state_p1 = (interrupt_state*) (kstack1.top<uint64_t>() - sizeof(interrupt_state));
	state_p1->rsp = ((uint64_t) state_p1) + sizeof(uint64_t); /* Because, after popping %rsp itself, it should point to the next register to be popped */

	state_p1->iregs.ss = 0x3b;
	state_p1->iregs.rsp = ustack_p1_top;
	state_p1->iregs.rflags = 0x202;
	state_p1->iregs.cs = 0x33;
	state_p1->iregs.rip = (uint64_t) &user_space;


	/* SETUP PS 2*/
	/* Place state at the top of the process' stack */
	state_p2 = (interrupt_state*) (kstack2.top<uint64_t>() - sizeof(interrupt_state));
	state_p2->rsp = ((uint64_t) state_p2) + sizeof(uint64_t); /* Because, after popping %rsp itself, it should point to the next register to be popped */

	state_p2->iregs.ss = 0x3b;
	state_p2->iregs.rsp = ustack_p2_top;
	state_p2->iregs.rflags = 0x202;
	state_p2->iregs.cs = 0x33;
	state_p2->iregs.rip = (uint64_t) &user_space2;


//	tss0.rsp0 = kstack2.top<uint64_t>();

	pic_sys.sti();
	switch_to(state_p1);

	panic("death");


//	kstack1 = (uint64_t) mallocator::malloc(4096);
//	kstack2 = (uint64_t) mallocator::malloc(4096);
	ustack2 = (uint64_t) mallocator::malloc(256);


//	tss0.rsp0 = kstack1 + 4096;
//	tsk[0].krsp = kstack1 + 4096;


//	tsk[1].krsp = kstack2 + 4096;
//	tsk[1].rsp = ustack2 + 256;

	pic_sys.sti();

	/// XXX We go wrong because of the stack pointer here?
//	for(;;);
	/* Never return from this */
	asm volatile(
			"mov $0x3b, %%ax\n"
			"mov %%ax, %%ds\n"
			"mov %%ax, %%es\n"
			"mov %%ax, %%fs\n"
			"mov %%ax, %%gs\n"

			"mov %%rsp, %%rax\n"

			"pushq $0x3b\n"	// data seg
			"pushq %%rax\n"	// stack pointer
//			"pushq $2\n" 	// XXX: Check what flags we need?
			"pushfq\n" 		// Push flags
			"pushq $0x33\n"	// code seg

			"pushq %%rbx\n"	// pointer

			"iretq\n"
			:
			: "b" (&user_space)
			:
			);


	for(;;)
	{
		asm volatile ("hlt\nhlt\nhlt\nhlt");
	}

	die();
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

