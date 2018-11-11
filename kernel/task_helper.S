.section .text

#define __ASSEMBLY__

#include "config.h"

.global uspace_jump_trampoline
.type uspace_jump_trampoline, @function
uspace_jump_trampoline:
	# Does nothing now. Will have to shuffle some registers once we
	# remove all the caller saved regs from being saved/loaded at state-switches
	jmp    jump_uspace

.global jump_uspace
.type jump_uspace, @function
jump_uspace:
	mov $USER_DS_PL3, %rax   # Setup data segment
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	pushq %rax         # Push ss
	pushq %rsi         # Push rsp (in second argument)
	pushq $202         # Push flags
	pushq $USER_CS_PL3 # Push cs
	pushq %rdi         # Push rip (in first argument)

	iretq              # Jump to userspace!

.global init_tsk0
.type init_tsk0, @function
init_tsk0:
	mov %rdx, %rsp		# Set stack pointer (in third argument)
	push %rsi			# Push return pointer (in second argument)
	jmp *%rdi			# Jump to task (address in first argument)


# Perform a state switch. Current state will be saved on the current stack frame before
# saving the stack pointer in save_rsp. Then, the new stack pointer will be loaded from
# rsp before finally restoring the state from the new stack frame.
# NOTE: If, for any reason, this function could be called to switch between identical stack
# frames, make sure that (&rsp == &save_rsp). I.e. if the scheduler schedules the same task
# twice in s row, use the actual rsp entry in the task structure as reference for both
# arguments, and don't use a copy of rsp! */
.global state_switch
.type state_switch, @function
state_switch:
# XXX: Wrap caller-saved registers in DEBUG ifdefs.
# Save registers of the current task onto the current stack
pushq %rax
pushq %rcx
	pushq %rdx
	pushq %rbx
	pushq %rbp
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	
	# Memory address of the stack pointer to save is in %rsi (second argument)
	mov %rsp, (%rsi)
	
	# Address of the new stack pointer is in %rdi (first argument)
	mov (%rdi), %rsp
	
	# Restore registers of the next task from the new stack
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rbp
	popq %rbx
	popq %rdx
	popq %rcx
	popq %rax

	# 'return' to the new task
	ret

