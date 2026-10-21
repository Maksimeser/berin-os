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

    ; Сохраняем текущий RSP в prev->kernel_rsp (смещение 16)
    mov [rdi + 16], rsp

    ; Загружаем RSP из next->kernel_rsp
    mov rsp, [rsi + 16]

    ; Восстанавливаем регистры
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

global task_ret_to_user
task_ret_to_user:
    ; На стеке лежит фрейм: SS, RSP, RFLAGS, CS, RIP
    ; Мы уже в Ring 0, стек ядра. iretq вернет нас в Ring 3.
    iretq
