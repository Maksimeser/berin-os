/* Berin OS - i386 IDT + interrupt dispatch */

#include "idt.h"

extern void berin_panic(uint16_t code, const char *name, struct regs *r);

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

/* from isr_stubs.asm */
extern uint32_t isr_stub_table[256];

static void idt_set_gate(int n, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[n].base_low  = (uint16_t)(base & 0xFFFF);
    idt[n].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[n].sel       = sel;
    idt[n].zero      = 0;
    idt[n].flags     = flags;
}

static void map_int_to_panic(uint32_t int_no, uint16_t *code, const char **name) {
    switch (int_no) {
        case  0: *code = 3;   *name = "divide_error";         return;
        case  1: *code = 0;   *name = "debug";                return;
        case  2: *code = 0;   *name = "nmi";                  return;
        case  3: *code = 0;   *name = "breakpoint";           return;
        case  4: *code = 0;   *name = "overflow";             return;
        case  5: *code = 0;   *name = "bound_range";          return;
        case  6: *code = 6;   *name = "invalid_opcode";       return;
        case  7: *code = 0;   *name = "device_not_available"; return;
        case  8: *code = 4;   *name = "double_fault";         return;
        case 10: *code = 0;   *name = "invalid_tss";          return;
        case 11: *code = 8;   *name = "segment_not_present";  return;
        case 12: *code = 7;   *name = "stack_segment";        return;
        case 13: *code = 2;   *name = "general_protection";   return;
        case 14: *code = 1;   *name = "page_fault";           return;
        case 16: *code = 0;   *name = "x87_fp";               return;
        case 17: *code = 9;   *name = "alignment_check";      return;
        case 18: *code = 10;  *name = "machine_check";        return;
        default: *code = 0;   *name = "unknown_error";        return;
    }
}

void isr_handler(struct regs *r) {
    uint16_t code;
    const char *name;
    map_int_to_panic(r->int_no, &code, &name);
    berin_panic(code, name, r);
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; i++)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);

    __asm__ __volatile__("lidt %0" : : "m"(idtp));
}
