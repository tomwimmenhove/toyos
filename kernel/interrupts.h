#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

/* Registers saved by PUSHER in interrupt_jump.S */
struct __attribute__((packed)) interrupt_saved_regs
{
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;

	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;
};

/* Registers pushed by the CPU when entering an interrupt */
struct __attribute__((packed)) interrupt_iretq_regs
{
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

struct __attribute__((packed)) exception_state
{   
	interrupt_saved_regs sregs;

	uint64_t rdi;		/* The argument to void exception_*(exception_state* state) */
	uint64_t err_code;	/* The error code pushed by the CPU (or the dummy pushed by the
						   interrupt_jump.S for exceptions that don't use error codes */

	interrupt_iretq_regs iregs;
};

struct __attribute__((packed)) interrupt_state
{   
	interrupt_saved_regs sregs;

	/* The two arguments to interrupt_handler(uint64_t irq_num, interrupt_state* state) */
	uint64_t rsi;
	uint64_t rdi;

	interrupt_iretq_regs iregs;
};

struct interrupts
{
	using irq_handler = void (*)(uint64_t irq_num, interrupt_state* state);

	static void init();

	static inline void regist(uint8_t intr, irq_handler handler) { irq_handlers[intr] = handler; }
	static inline void unregist(uint8_t intr) { irq_handlers[intr] = nullptr; }
	static inline void handle(uint64_t irq_num, interrupt_state* state)
	{ 
		auto handler = irq_handlers[irq_num];
		if (handler)
			handler(irq_num, state);
	} 

	static irq_handler irq_handlers[256];
};


extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state);

#endif /* INTERRUPTS_H */
