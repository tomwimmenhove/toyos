.global _start
.type _start, @function
_start:
	push %rbp
	mov %rsp, %rbp
	mov $0x800000000000, %rsp
	call main
	mov %rbp, %rsp
	pop %rbp
	ret

