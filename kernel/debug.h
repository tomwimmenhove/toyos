#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include "console.h"
#include "io.h"
#include "interrupts.h"

void die() __attribute__ ((noreturn));
void panic(const char* msg, interrupt_state* regs = nullptr) __attribute__ ((noreturn));

inline void qemu_out_char(char ch) { outb(ch, 0xe9); }

#endif /* DEBUG_H */
