#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

struct irq
{
	virtual uint8_t to_intr(uint8_t irq) = 0;
};

#endif /* IRQ_H */
