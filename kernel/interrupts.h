#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

struct __attribute__((packed)) interrupt_state
{   
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;

	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;

	uint64_t rsi;
	uint64_t rdi;

	uint64_t err_code;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state);

#endif /* INTERRUPTS_H */
