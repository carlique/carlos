.global isr_stub_6
.global isr_stub_8
.global isr_stub_13
.global isr_stub_14
.extern isr_common_handler

// For vectors without CPU error code: push 0 error, then vector
.macro ISR_NOERR vec
isr_stub_\vec:
  pushq $0
  pushq $\vec
  jmp isr_enter
.endm

// For vectors with CPU error code: CPU already pushed error, then RIP,CS,RFLAGS
// We push only vector so stack becomes: [vector][error][rip][cs][rflags]
.macro ISR_ERR vec
isr_stub_\vec:
  pushq $\vec
  jmp isr_enter
.endm

isr_enter:
  // Pass pointer to our frame (at current RSP)
  mov %rsp, %rdi

  // SysV ABI: stack must be 16-byte aligned *before* call
  sub $8, %rsp
  call isr_common_handler
  add $8, %rsp

  cli
1: hlt
   jmp 1b

ISR_NOERR 6
ISR_ERR   8
ISR_ERR   13
ISR_ERR   14