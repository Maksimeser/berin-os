; Berin - x86_64 boot
section .note.Xen progbits alloc noexec nowrite align=4
    align 4
    dd 4
    dd 8
    dd 18
    db "Xen", 0
    align 4
    dq _start
    align 4

section .text
bits 32
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    mov ebp, esp

    call setup_paging

    lgdt [gdt64_ptr]

    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    jmp dword 0x08:long_mode_entry

setup_paging:
      mov eax, pdpt
    or  eax, 0x07
    mov [pml4], eax
    mov dword [pml4 + 4], 0

    ; PDPT[0] -> PD
    mov eax, pd
    or  eax, 0x07
    mov [pdpt], eax

    xor ecx, ecx
.fill_pd:
    mov eax, ecx
    shl eax, 21
    or  eax, 0x87
    mov [pd + ecx * 8], eax
    mov dword [pd + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .fill_pd

    mov eax, pml4
    mov cr3, eax
    ret

bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top
    xor rbp, rbp

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .rodata
align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:

align 4096
pml4: resb 4096
pdpt: resb 4096
pd:   resb 4096
