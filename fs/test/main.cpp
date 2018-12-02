#include "syscalls.h"

extern "C" int _start()
{
	int fd = open(0, 0);

	write(fd, (void*) "USERSPACEBIN: Hello world\n", 26);

	return 42;
}
