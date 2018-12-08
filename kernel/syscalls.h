#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include "syscall.h"

enum class syscall_idx
{
	debug_outs,
	debug_outc,
	yield,
	stop,
	debug_test_timer,
	debug_test_read1,
	debug_test_read2,
	debug_test_write,
	debug_clone_tlb,

	mount_cd,

	open_file,
	open_dev,
	close,
	read,
	write,
	seek,
	fsize,

	fmap,
	funmap,
};

extern "C"
{
	inline int close(int arg0) { return (bool) syscall((int) syscall_idx::close, arg0); }
	inline ssize_t read(int fd, void* buf, size_t len) { return syscall((int) syscall_idx::read, fd, (uint64_t) buf, len); }
	inline ssize_t write(int fd, void* buf, size_t len) { return syscall((int) syscall_idx::write, fd, (uint64_t) buf, len); }
}

/* Non-standard */
inline void yield() { syscall((int) syscall_idx::yield); }
inline int open(int arg0, int arg1) { return (int) syscall((int) syscall_idx::open_dev, arg0, arg1); }
inline int open(const char* name) { return (int) syscall((int) syscall_idx::open_file, (uint64_t) name); }
inline size_t fsize(int fd) { return syscall((int) syscall_idx::fsize, fd); }
inline size_t seek(int fd, size_t pos) { return syscall((int) syscall_idx::seek, fd, pos); }

inline ssize_t fmap(int fd, uint64_t file_offs, uint64_t addr, uint64_t len)
{
	return syscall((int) syscall_idx::fmap, fd, file_offs, addr, len);
}

inline ssize_t funmap(int fd, uint64_t addr)
{
	return syscall((int) syscall_idx::funmap, fd, addr);
}

#endif /* SYSCALLS_H */
