#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

extern "C"
{
	inline int close(int arg0) { return (bool) syscall(0x11, arg0); }
	inline ssize_t read(int fd, void* buf, size_t len) { return syscall(0x13, fd, (uint64_t) buf, len); }
	inline ssize_t write(int fd, void* buf, size_t len) { return syscall(0x14, fd, (uint64_t) buf, len); }
}

/* Non-standard */
inline void yield() { syscall(5); }
inline int open(int arg0, int arg1) { return (int) syscall(0x10, arg0, arg1); }
inline int open(const char* name) { return (int) syscall(0x0d, (uint64_t) name); }
inline size_t fsize(int fd) { return syscall(0x16, fd); }
inline size_t seek(int fd, size_t pos) { return syscall(0x17, fd, pos); }

inline size_t fmap(int fd, uint64_t file_offs, uint64_t addr, uint64_t len)
{
	return syscall(0x15, fd, file_offs, addr, len);
}

#endif /* SYSCALLS_H */
