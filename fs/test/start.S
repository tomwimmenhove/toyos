.global _start
.type _start, @function
_start:
	mov $0x800000000000, %rsp
	jmp main

