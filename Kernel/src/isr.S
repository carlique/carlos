// isr.S — x86_64 exception stubs 0..31
// Produces global symbols: isr0 .. isr31
// Calls: isr_common_handler(IsrFrame *f)
// Frame matches your IsrFrame:
//   vector, error, rip, cs, rflags
//
// Notes:
// - For NOERR exceptions: we push error=0, then vector.
// - For ERR exceptions: CPU already pushed error; we push vector only.
// - We also save/restore GPRs around the call.
// - Stack alignment: ensure RSP%16==8 before call.

.global isr0
.global isr1
.global isr2
.global isr3
.global isr4
.global isr5
.global isr6
.global isr7
.global isr8
.global isr9
.global isr10
.global isr11
.global isr12
.global isr13
.global isr14
.global isr15
.global isr16
.global isr17
.global isr18
.global isr19
.global isr20
.global isr21
.global isr22
.global isr23
.global isr24
.global isr25
.global isr26
.global isr27
.global isr28
.global isr29
.global isr30
.global isr31

.extern isr_common_handler

// Save/restore GPRs (SysV ABI safety)
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

// bytes pushed by PUSH_GPRS: 15 regs * 8 = 120
.set GPRS_SIZE, 120

// For exceptions WITHOUT error code, we push error=0, then vector.
// Stack after pushes (top -> down):
//   r15..rax (120 bytes), vector, 0, rip, cs, rflags
// We pass pointer to (vector, error, rip, cs, rflags) == IsrFrame
.macro ISR_NOERR vec
isr\()\vec:
  pushq $0
  pushq $\vec
  PUSH_GPRS

  // ---- stack alignment for C call ----
  // Want RSP%16==8 before call.
  // Use r12 as scratch "pad size" (restored by POP_GPRS).
  xorq %r12, %r12
  movq %rsp, %rax
  andq $0xF, %rax
  cmpq $8, %rax
  je 1f
  subq $8, %rsp
  movq $8, %r12
1:
  // rdi = &IsrFrame (skip saved regs, and optional pad)
  leaq (GPRS_SIZE)(%rsp,%r12,1), %rdi
  call isr_common_handler

  // undo optional pad
  addq %r12, %rsp

  POP_GPRS
  addq $16, %rsp     // drop vector + error(0)
  iretq
.endm

// For exceptions WITH error code, CPU already pushed error.
// Stack on entry: error, rip, cs, rflags
// We push vector only.
// After pushes:
//   r15..rax (120), vector, error, rip, cs, rflags
.macro ISR_ERR vec
isr\()\vec:
  pushq $\vec
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
  call isr_common_handler

  addq %r12, %rsp
  POP_GPRS
  addq $8, %rsp      // drop vector only (CPU error remains)
  iretq
.endm

// 0..31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31