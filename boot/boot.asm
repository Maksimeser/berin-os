MB1_MAGIC    equ 0x1BADB002
MB1_FLAGS    equ 0x00000000
MB1_CHECKSUM equ -(MB1_MAGIC + MB1_FLAGS)

section .multiboot_header
align 4
    dd MB1_MAGIC
    dd MB1_FLAGS
    dd MB1_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    mov ebp, esp

    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
