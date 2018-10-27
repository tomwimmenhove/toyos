ENTRY(_start)

SECTIONS
{
   . = 0x1000000;
   .text : { *(.text) }
   . = 0x1800000;
   .data : { *(.data) }
   .bss : { *(.bss) }
   . = ALIGN(8); 
   _data_end = .; 
}
