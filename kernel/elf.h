#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

struct elf64
{
	elf64(int fd);

	bool load_headers();

	struct __attribute__((packed)) elf64_ehdr
	{   
		unsigned char e_ident[16]; /* ELF identification */
		uint16_t e_type; /* Object file type */
		uint16_t e_machine; /* Machine type */
		uint32_t e_version; /* Object file version */
		uint64_t e_entry; /* Entry point address */
		uint64_t e_phoff; /* Program header offset */
		uint64_t e_shoff; /* Section header offset */
		uint32_t e_flags; /* Processor-specific flags */
		uint16_t e_ehsize; /* ELF header size */
		uint16_t e_phentsize; /* Size of program header entry */
		uint16_t e_phnum; /* Number of program header entries */
		uint16_t e_shentsize; /* Size of section header entry */
		uint16_t e_shnum; /* Number of section header entries */
		uint16_t e_shstrndx; /* Section name string table index */
	};

	struct __attribute__((packed)) elf64_phdr
	{   
		uint32_t p_type; /* Type of segment */
		uint32_t p_flags; /* Segment attributes */
		uint64_t p_offset; /* Offset in file */
		uint64_t p_vaddr; /* Virtual address in memory */
		uint64_t p_paddr; /* Reserved */
		uint64_t p_filesz; /* Size of segment in file */
		uint64_t p_memsz; /* Size of segment in memory */
		uint64_t p_align; /* Alignment of segment */
	};

	elf64_ehdr ehdr;

private:
	int fd;
};



#endif /* ELF_H */
