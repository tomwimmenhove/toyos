#include "elf.h"
#include "console.h"
#include "syscall.h"
#include "syscalls.h"
#include "klib.h"

elf64::elf64(int fd)
	: fd(fd)
{ }

bool elf64::load_headers()
{
	console_user ucon(0);

	seek(fd, 0);

	if (read(fd, (void*) &ehdr, sizeof(ehdr)) == -1)
		return false;

	/* Check magic */
	if (memcmp((void*) ehdr.e_ident, (void*) "\x7f" "ELF", 4) != 0)
		return false;

	/* Check for ELFCLASS64 */
	if (ehdr.e_ident[4] != 2)
		return false;

	ucon << "entry: " << hex_u64(ehdr.e_entry) << '\n';

	for (int i = 0; i < ehdr.e_phnum; i++)
	{
		elf64_phdr phdr;

		if (read(fd, (void*) &phdr, sizeof(phdr)) == -1)
			return false;

		switch(phdr.p_type)
		{
			case 0: /* Unised */
				break;
			case 1: /* Loadable segment */
				if (fmap(fd,
							phdr.p_offset & ~(PAGE_SIZE - 1),
							phdr.p_vaddr & ~(PAGE_SIZE - 1),
							phdr.p_filesz + (phdr.p_offset & (PAGE_SIZE - 1))) == -1)
					return -1;

				break;
			case 2: /* Dynamic linking tables */
				break;
			case 3: /* Program interpreter path name\ */
				break;
			case 4: /* Note sections */
				break;
			default: /* Unknown. Ignored */
				break;
		}
	}
	return true;
}

