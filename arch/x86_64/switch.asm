section .text
bits 64

global switch_to
switch_to:
    ; rdi = prev, rsi = next
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Сохраняем текущий kernel_rsp в prev (offset 168)
    mov [rdi + 168], rsp

    ; Загружаем kernel_rsp из next
    mov rsp, [rsi + 168]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
