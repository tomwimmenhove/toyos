#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

struct irq
{
	virtual uint8_t to_intr(uint8_t irq) = 0;

	virtual void sti() = 0;
	virtual void cli() = 0;
};

struct irq_x86 : public irq
{
	inline void sti() override { asm volatile("sti"); }
	inline void cli() override { asm volatile("cli"); }
};

#endif /* IRQ_H */
