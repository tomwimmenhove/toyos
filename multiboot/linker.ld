OUTPUT_FORMAT("elf32-i386", "elf32-i386",
          "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)

_stack_size = 0x10000;

SECTIONS
{
   . = 0x100000;
   .text : { *(.text) }

   . = 0x120000;
   .data : { *(.data) }

   . = ALIGN(8);
   .bss : { *(.bss) }

   . = ALIGN(8);
   _stack_bottom = .;
   . = . + _stack_size;
   _stack_top = .;

   . = ALIGN(0x1000);
   _data_end = .; 
}
