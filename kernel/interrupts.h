#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

/* Registers pushed by the CPU when entering an interrupt */
struct __attribute__((packed)) interrupt_iretq_regs
{
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

struct __attribute__((packed)) interrupt_state
{   
	uint64_t rsp;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	/* The two arguments to interrupt_handler(uint64_t irq_num, interrupt_state* state) */
	uint64_t rsi;
	uint64_t rdi;
	uint64_t err_code;	/* The error code pushed by the CPU (or the dummy pushed by the
						   interrupt_jump.S for exceptions that don't use error codes */

	interrupt_iretq_regs iregs;
};

struct intr_regist
{
	using irq_handler = void (*)(uint64_t irq_num, interrupt_state* state);
	bool run_scheduler;
	irq_handler handler;
};

struct interrupts
{
	static void init();

	static void regist(uint8_t intr, intr_regist::irq_handler handler, bool run_scheduler = false);
	static void unregist(uint8_t intr);
	static void handle(uint64_t irq_num, interrupt_state* state);
	static void do_reschedule();

private:
	static intr_regist registrars[256];
	static int nest;
	static bool reschedule;
};

extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state);

#endif /* INTERRUPTS_H */
