.section .text

.global __syscall1
.type __syscall1, @function
.global __syscall2
.type __syscall2, @function
.global __syscall3
.type __syscall3, @function
.global __syscall4
.type __syscall4, @function
.global __syscall5
.type __syscall5, @function
.global __syscall6
.type __syscall6, @function
__syscall1:
__syscall2:
__syscall3:
__syscall4:
__syscall5:
__syscall6:
	int $42
	ret

