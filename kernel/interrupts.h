#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>
#include <forward_list>

#include "new.h"

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

struct intr_driver;

struct interrupts
{
	static void init();

	static void add(uint8_t intr, intr_driver* driver) { drivers[intr].push_front(driver); }
	static void remove(uint8_t intr, intr_driver* driver) { drivers[intr].remove(driver); }

	static void handle(uint64_t irq_num, interrupt_state* state);
	static void do_reschedule();

	static inline uint64_t save_flags()
	{
		uint64_t flags;
		asm volatile("pushf\n"
					 "pop %0\n"
					 : "=r" (flags));

		return flags;
	}

	static inline void restore_flags(uint64_t flags)
	{
		asm volatile("push %0\n"
					 "popf\n"
					 : : "r" (flags));
	}

	static inline void sti() { asm volatile("sti"); }
	static inline void cli() { asm volatile("cli"); }

	static inline uint64_t cli_store()
	{
		uint64_t flags = save_flags();
		cli();

		return flags;
	}

private:
	static std::forward_list<intr_driver*> drivers[256];
	static int nest;
	static bool reschedule;
};

struct intr_driver
{
	intr_driver(uint8_t intr)
		: intr(intr)
	{
		interrupts::add(intr, this);
	}

	virtual void interrupt(uint64_t irq_num, interrupt_state* state) = 0;
	bool run_scheduler = false;

	virtual ~intr_driver()
	{
		interrupts::remove(intr, this);
	}

private:
	uint8_t intr;
};

extern "C" void interrupt_handler(uint64_t irq_num, interrupt_state* state);

#endif /* INTERRUPTS_H */
