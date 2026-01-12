// src/irq.S — IRQ stubs 0..15 (vectors 0x20..0x2F)
// Calls: irq_common_handler(IsrFrame*)
// Layout identical to ISR_NOERR (push error=0, then vector)

.global irq0
.global irq1
.global irq2
.global irq3
.global irq4
.global irq5
.global irq6
.global irq7
.global irq8
.global irq9
.global irq10
.global irq11
.global irq12
.global irq13
.global irq14
.global irq15

.extern irq_common_handler

.macro PUSH_GPRS
  pushq %rax
  pushq %rcx
  pushq %rdx
  pushq %rbx
  pushq %rbp
  pushq %rsi
  pushq %rdi
  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
.endm

.macro POP_GPRS
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %r11
  popq %r10
  popq %r9
  popq %r8
  popq %rdi
  popq %rsi
  popq %rbp
  popq %rbx
  popq %rdx
  popq %rcx
  popq %rax
.endm

.set GPRS_SIZE, 120

.macro IRQ_STUB vec
irq\()\vec:
  pushq $0
  pushq $(0x20+\vec)
  PUSH_GPRS

  xorq %r12, %r12
  movq %rsp, %rax
  andq $0xF, %rax
  cmpq $8, %rax
  je 1f
  subq $8, %rsp
  movq $8, %r12
1:
  leaq (GPRS_SIZE)(%rsp,%r12,1), %rdi
  call irq_common_handler

  addq %r12, %rsp
  POP_GPRS
  addq $16, %rsp
  iretq
.endm

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15