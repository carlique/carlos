.global _start
.extern kmain

.section .text
_start:
  lea stack_top(%rip), %rsp
  and $-16, %rsp
  call kmain

.hang:
  hlt
  jmp .hang

.section .bss
.align 16
stack_bottom:
  .skip 16384
stack_top: