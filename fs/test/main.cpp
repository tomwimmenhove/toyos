#include "syscalls.h"
#include "klib.h"
#include "printf.h"

extern "C" void _putchar(char ch)
{
	write(0, (void*) &ch, 1);
}

extern "C" int main()
{
	open(0, 0);

	printf("USERSPACEBIN: Hello world\n");

	uint8_t abuf[512];
	uint8_t ata_buf1[512];

	syscall(syscall_idx::debug_test_read2, (uint64_t) ata_buf1, 0, 512);

	const char* fname = "bla/othefile.txt";
	int fd = open(fname);
	printf("fd: %d\n", fd);
	ssize_t len = fsize(fd);
	printf("len: %d\n", len);
#if 1
	char* d = (char*) 0x20000;
	int r = fmap(fd, 0, (uint64_t) d, len);
	printf("map result: %d\n", r);
	write(0, d, len);
#endif

	printf("before close\n");
	close(fd);
#if 1
	printf("before unmap\n");
	funmap(fd, (uint64_t) d);
#endif
	printf("all closed\n");

	//int c = 0;
	for (int i = 0; i < 1000; i++)
	//for(;;)
	{   
		syscall(syscall_idx::debug_test_read2, (uint64_t) abuf, 0, 512);

		for (int i = 0; i < 512; i++)
		{
			if (abuf[i] != ata_buf1[i])
				printf("ATA: KAPOT\n");
		}

		//printf("c=%d\n", c++);
	}

	return 42;
}
