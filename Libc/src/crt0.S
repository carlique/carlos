.global _start
.extern main
.extern carlos_api

_start:
  // RDI = CarlosApi*
  mov %rdi, carlos_api(%rip)

  call main

  // main returns int in EAX, call api->exit(code)
  mov carlos_api(%rip), %rbx
  mov %eax, %edi
  call *8(%rbx)     // exit function pointer (after abi_version and write)
  hlt