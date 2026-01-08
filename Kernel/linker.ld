ENTRY(_start)

SECTIONS
{
  . = 0x100000;
  __kernel_start = 0x100000;

  .text : ALIGN(16) { *(.text .text.*) }
  .rodata : ALIGN(16) { *(.rodata .rodata.*) }
  .data : ALIGN(16) { *(.data .data.*) }
  .bss : ALIGN(16) { *(COMMON) *(.bss .bss.*) }

  __kernel_end = .;

  /DISCARD/ : { *(.eh_frame) *(.note*) }
}