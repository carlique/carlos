.global exec_enter
.global carlos_kexit

.extern g_exec_saved_rsp
.extern g_exec_exit_code

// int exec_enter(void *entry, void *stack_top, void *api, int argc, char **argv);
// SysV on entry:
//   rdi=entry, rsi=stack_top, rdx=api, rcx=argc, r8=argv
exec_enter:
  mov %rsp, g_exec_saved_rsp(%rip)

  mov %rdi, %rbx          // entry
  mov %rsi, %rsp          // switch stack
  and $-16, %rsp
  sub $8, %rsp            // SysV: rsp%16==8 before call

  // Call program entry as: _start(api, argc, argv)
  mov %rdx, %rdi          // api  -> RDI
  mov %rcx, %rsi          // argc -> RSI
  mov %r8,  %rdx          // argv -> RDX

  call *%rbx              // jump to program entry

  // if returns, treat as exit(0)
  movl $0, g_exec_exit_code(%rip)
  mov g_exec_saved_rsp(%rip), %rsp
  movl g_exec_exit_code(%rip), %eax
  ret

// void carlos_kexit(int code)
carlos_kexit:
  movl %edi, g_exec_exit_code(%rip)
  mov g_exec_saved_rsp(%rip), %rsp
  movl g_exec_exit_code(%rip), %eax
  ret