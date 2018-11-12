#ifndef TASK_HELPER_H
#define TASK_HELPER_H

#include <stdint.h>

struct __attribute__((packed)) switch_regs
{
	/* XXX: Wrap caller-saved registers in DEBUG ifdefs. */

	/* Callee saved registers */
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;

#ifdef TASK_SWITCH_SAVE_CALLER_SAVED
	/* Caller saved registers */
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;
#endif

	uint64_t rip;

	/* Not really a register, but very handy to
	 *      * be able to set a return address. */
	uint64_t ret_ptr;
};


extern "C" void uspace_jump_trampoline();
extern "C" void jump_uspace(uint64_t rip, uint64_t rsp);
extern "C" void init_tsk0(uint64_t rip, uint64_t ret, uint64_t rsp);
extern "C" void state_switch(uint64_t& rsp, uint64_t& save_rsp);

#endif /* TASK_HELPER_H */
