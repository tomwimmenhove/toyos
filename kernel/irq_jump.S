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

# For interrupts that push no error code, push a dummy onto the stack
.macro IRQ_NO_ERRCODE n
.global irq_\n
.type irq_\n, @function
irq_\n:
	pushq $0
	push %rdi
	push %rsi
	mov $\n, %rdi
	jmp irq_body
.align 0x10
.endm

.macro IRQ_ERRCODE n
.global irq_\n
.type irq_\n, @function
irq_\n:
	push %rdi
	push %rsi
	mov $\n, %rdi
	jmp irq_body
.align 0x10
.endm

.set irq_n, 0
.rept 256
.if irq_n == 8 || irq_n == 10 ||  irq_n == 11 || irq_n == 12 || irq_n == 13 || irq_n == 14 || irq_n == 17
	IRQ_ERRCODE %irq_n
.else
	IRQ_NO_ERRCODE %irq_n
.endif
	.set irq_n, irq_n+1
.endr

irq_body:
	PUSHER
	cld
	mov %rsp, %rsi
	call interrupt_handler
	POPPER
	pop %rsi
	pop %rdi
	add $8, %rsp
	iretq

