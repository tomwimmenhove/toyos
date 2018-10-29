ENTRY(_start)

SECTIONS
{
/*	. = 0xffff800000000000; <<=- Why would this not work? */
	. = 0xffffffff80000000;
	.text : { *(.text) }

	. = ALIGN(0x1000); 
	.data : { *(.data) }

	. = ALIGN(0x1000); 

	.rodata : { *(.rodata*) }

	. = ALIGN(0x1000); 
	.bss : { *(.bss) }

	. = ALIGN(8); 
	_data_end = .; 
}
