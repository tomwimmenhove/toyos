#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

extern "C"
{
	inline void yield() { syscall(5); }
	inline int open(int arg0, int arg1) { return (int) syscall(0x10, arg0, arg1); }
	inline int close(int arg0) { return (bool) syscall(0x11, arg0); }
	inline size_t read(int fd, void* buf, size_t len) { return syscall(0x13, fd, (uint64_t) buf, len); }
	inline size_t write(int fd, void* buf, size_t len) { return syscall(0x14, fd, (uint64_t) buf, len); }
}

#endif /* SYSCALLS_H */
