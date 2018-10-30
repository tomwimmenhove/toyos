ENTRY(_start)

SECTIONS
{
	. = 0xffffffff80000000;
	.text : { *(.text) }

	. = ALIGN(8); 
	.data : { *(.data) }

	. = ALIGN(8); 
	.rodata : { *(.rodata*) }

	. = ALIGN(0x1000); 
	.bss : { *(.bss) }

	. = ALIGN(0x1000); 
	_data_end = .; 
}
