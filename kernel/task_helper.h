#ifndef TASK_HELPER_H
#define TASK_HELPER_H

#include <stdint.h>

extern "C" void uspace_jump_trampoline(uint64_t rdi, uint64_t rsi);
extern "C" void jump_uspace(uint64_t rip, uint64_t rsp);
extern "C" void init_tsk0(uint64_t rip, uint64_t ret, uint64_t rsp);
extern "C" void state_switch(uint64_t& rsp, uint64_t& save_rsp);

#endif /* TASK_HELPER_H */
