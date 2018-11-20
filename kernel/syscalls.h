#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

inline int open(int arg0, int arg1) { return (int) syscall(0x10, arg0, arg1); }
inline size_t read(int fd, int idx, void* buf, size_t len) { return syscall(0x13, fd, idx, (uint64_t) buf, len); }
inline size_t write(int fd, int idx, void* buf, size_t len) { return syscall(0x14, fd, idx, (uint64_t) buf, len); }

#endif /* SYSCALLS_H */
