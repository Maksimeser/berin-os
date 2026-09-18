#ifndef BERIN_I386_IDT_H
#define BERIN_I386_IDT_H

#include <stdint.h>

/* Pushed by isr_common in isr_stubs.asm — order matters! */
struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
};

void idt_init(void);

#endif
