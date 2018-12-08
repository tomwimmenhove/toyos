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

	syscall(10, (uint64_t) ata_buf1, 0, 512);

	//int c = 0;
	for (int i = 0; i < 1000; i++)
	//for(;;)
	{   
		syscall(10, (uint64_t) abuf, 0, 512);

		for (int i = 0; i < 512; i++)
		{
			if (abuf[i] != ata_buf1[i])
				printf("ATA: KAPOT\n");
		}

		//printf("c=%d\n", c++);
	}

	return 42;
}
