.code32

.section .text
.align 4
.global jump_kernel
.type jump_kernel, @function
jump_kernel: 
	/* Set data segment */
	mov $0x10, %ecx
	mov %ecx, %ds
	mov %ecx, %es
	mov %ecx, %fs
	mov %ecx, %gs
	mov %ecx, %ss

	/* Jump to our new code segment in long mode */
	jmp $0x8, $jump64
.code64
jump64:
	/* Get our kernel entry point in %rax */
	movq 4(%rsp), %rax

	/* Pointer to the multiboot structure */
	movq 20(%rsp), %rdi

	/* Get our stack pointer */
	movq 12(%rsp), %rsp /* Heh :) */

	/* Jump to it! */
	jmp *%rax
.code32

