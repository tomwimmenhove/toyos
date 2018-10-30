#ifndef JUMP_H
#define JUMP_H

#include <stdint.h>

//void jump_kernel(uint64_t entry, uint64_t stack_top, uint64_t mb_addr);
void jump_kernel(uint64_t entry, uint64_t stack_top);

#endif /* JUMP_H */
