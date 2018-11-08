.code32

.section .multiboot
.align 4
multiboot_header_start:
# multiboot v2
	.long 0xe85250d6                						# Magic
	.long 0                         						# Arch (i386)
        .long multiboot_header_end - multiboot_header_start				# Header length
	.long -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))	# Checksum


	/* From stackoverflow */
# tags
# module align
#	.word 6 # type
#	.word 0 # flags
#	.long 8 # size in bytes (spec says 12?)
#	.balign 8
# info request 
	.word 1
	.word 0
	.long 4*7+8
	.long 8 # framebuffer
	.long 6 # memory map
	.long 5 # BIOS boot device
	.long 1 # command line
	.long 3 # modules
	.long 9 # ELF symbols
	.long 10 # APM table
	.balign 8
	/* /from satackoverflow */

	.word 0
	.word 0
	.long 8

multiboot_header_end:

	.byte 0x42	# avoid _start == multiboot_header_end:

.set EDATA, data_end

.section .text
.align 4
.global _start
.type _start, @function
_start: 
	cli
	mov	$_stack_top, %esp
//	mov	$0x200000, %ebp

	pushl	%ebx
	pushl	%eax
	call	c_entry

	cli
	movl $0, %eax
	out %eax, $0xf4
_halt:
	hlt
	jmp _halt


