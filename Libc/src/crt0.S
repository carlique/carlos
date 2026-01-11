.global _start
.extern main
.extern carlos_api

.set CARLOS_API_WRITE_OFF,      8
.set CARLOS_API_EXIT_OFF,      16
.set CARLOS_API_ALLOC_OFF,     24
.set CARLOS_API_FREE_OFF,      32

_start:
  // RDI = CarlosApi*
  // RSI = argc
  // RDX = argv
  mov %rdi, carlos_api(%rip)
  
  // call main(argc, argv)
  mov %rsi, %rdi      // argc -> RDI
  mov %rdx, %rsi      // argv -> RSI

  call main

  // main returns int in EAX, call api->exit(code)
  mov carlos_api(%rip), %rbx
  mov %eax, %edi
  call *CARLOS_API_EXIT_OFF(%rbx)

  ret