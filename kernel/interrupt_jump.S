.section .text

.altmacro

.macro PUSHER
	push %rax
	push %rcx
	push %rdx

	push %r12
	push %r13
	push %r14
	push %r15
.endm

.macro POPPER
	pop %r15
	pop %r14
	pop %r13
	pop %r12

	pop %rdx
	pop %rcx
	pop %rax
.endm

# Exception that push no error code: push a dummy error code onto the stack
.macro INTR_EXCEPT_NO_ERRCODE n body
.global intr_\n
.type intr_\n, @function
intr_\n:
	pushq $0
	push %rdi
	jmp \body
.align 0x10
.endm

# Exceptions that DO push an error code
.macro INTR_EXCEPT_ERRCODE n body
.global intr_\n
.type intr_\n, @function
intr_\n:
	push %rdi
	jmp \body
.align 0x10
.endm

# Normal interrupts: move the interrupt number into the first argument
.macro INTR_NORMAL n body
.global intr_\n
.type intr_\n, @function
intr_\n: 
	push %rdi
	push %rsi
	mov $\n, %rdi
jmp \body
.align 0x10
.endm

# Normal interrupt handler body
.macro BODY_NORMAL name target
\name:
	PUSHER
	cld
	mov %rsp, %rsi
	call \target
	POPPER
	pop %rsi
	pop %rdi
	iretq
.endm

# Exception body handler: fix the stack (error code had been pushed))
.macro BODY_EXCEPT name target
\name:
	PUSHER
	cld
	mov %rsp, %rdi
	call \target
	POPPER
	pop %rdi
	add $8, %rsp
	iretq
.endm

# define the 'normal' interrupt body handler
BODY_NORMAL intr_body interrupt_handler

# define the exception body handlers
BODY_EXCEPT intr_body_unhandled exception_unhandled
BODY_EXCEPT intr_body_div_by_zero exception_div_by_zero
BODY_EXCEPT intr_body_debug exception_debug
BODY_EXCEPT intr_body_nmi exception_nmi
BODY_EXCEPT intr_body_breakpoint exception_breakpoint
BODY_EXCEPT intr_body_ovf exception_ovf
BODY_EXCEPT intr_body_bound exception_bound
BODY_EXCEPT intr_body_ill exception_ill
BODY_EXCEPT intr_body_dev_not_avail exception_dev_not_avail
BODY_EXCEPT intr_body_double exception_double
BODY_EXCEPT intr_body_invalid_tss exception_invalid_tss
BODY_EXCEPT intr_body_seg_not_present exception_seg_not_present
BODY_EXCEPT intr_body_stack_seg exception_stack_seg
BODY_EXCEPT intr_body_gp exception_gp
BODY_EXCEPT intr_body_fp exception_fp
BODY_EXCEPT intr_body_align exception_align
BODY_EXCEPT intr_body_mach_chk exception_mach_chk
BODY_EXCEPT intr_body_simd_fp exception_simd_fp
BODY_EXCEPT intr_body_virt exception_virt
BODY_EXCEPT intr_body_sec exception_sec
BODY_EXCEPT intr_body_page exception_page

# --- Done setting things up. Following are the actual 'entries' in our list

# Since all entries are 0x10 byte apart we MUST align here.
.align 0x10

# All exception entries
INTR_EXCEPT_NO_ERRCODE 0 intr_body_div_by_zero
INTR_EXCEPT_NO_ERRCODE 1 intr_body_debug
INTR_EXCEPT_NO_ERRCODE 2 intr_body_nmi
INTR_EXCEPT_NO_ERRCODE 3 intr_body_breakpoint
INTR_EXCEPT_NO_ERRCODE 4 intr_body_ovf
INTR_EXCEPT_NO_ERRCODE 5 intr_body_bound
INTR_EXCEPT_NO_ERRCODE 6 intr_body_ill
INTR_EXCEPT_NO_ERRCODE 7 intr_body_dev_not_avail
INTR_EXCEPT_ERRCODE 8 intr_body_double
INTR_EXCEPT_NO_ERRCODE 9 intr_body_unhandled
INTR_EXCEPT_ERRCODE 10 intr_body_invalid_tss
INTR_EXCEPT_ERRCODE 11 intr_body_seg_not_present
INTR_EXCEPT_ERRCODE 12 intr_body_stack_seg
INTR_EXCEPT_ERRCODE 13 intr_body_gp
INTR_EXCEPT_ERRCODE 14 intr_body_page
INTR_EXCEPT_NO_ERRCODE 15 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 16 intr_body_fp
INTR_EXCEPT_ERRCODE 17 intr_body_align
INTR_EXCEPT_NO_ERRCODE 18 intr_body_mach_chk
INTR_EXCEPT_NO_ERRCODE 19 intr_body_simd_fp
INTR_EXCEPT_NO_ERRCODE 20 intr_body_virt
INTR_EXCEPT_NO_ERRCODE 21 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 22 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 23 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 24 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 25 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 26 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 27 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 28 intr_body_unhandled
INTR_EXCEPT_NO_ERRCODE 29 intr_body_unhandled
INTR_EXCEPT_ERRCODE 30 intr_body_sec
INTR_EXCEPT_NO_ERRCODE 31 intr_body_unhandled

# The rest are 'normal' interrupts
.set intr_n, 32
.rept (256 - 32)
	INTR_NORMAL %intr_n intr_body
	.set intr_n, intr_n+1
.endr


