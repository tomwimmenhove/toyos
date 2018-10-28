ENTRY(_start)

SECTIONS
{
   . = 0xffff800000000000;
   .text : { *(.text) }
   . = 0xffff800000800000;
   .data : { *(.data) }
   .bss : { *(.bss) }
   . = ALIGN(8); 
   _data_end = .; 
}
