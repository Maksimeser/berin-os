/* Berin OS - i386 GDT (flat memory model) */

#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gp;

static void set_gate(int i, uint32_t base, uint32_t limit,
                     uint8_t access, uint8_t gran) {
    gdt[i].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));

    gdt[i].access      = access;
                     }

                     void gdt_init(void) {
                         gp.limit = sizeof(gdt) - 1;
                         gp.base  = (uint32_t)&gdt;

                         /* 0: null descriptor */
                         set_gate(0, 0, 0, 0x00, 0x00);

                         /* 1: kernel code  - base 0, limit 4G, ring0, RX, 4K gran, 32-bit */
                         set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);

                         /* 2: kernel data  - base 0, limit 4G, ring0, RW, 4K gran, 32-bit */
                         set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

                         __asm__ __volatile__(
                             "lgdt %0\n"
                             "ljmp $0x08, $1f\n"
                             "1:\n"
                             "mov $0x10, %%ax\n"
                             "mov %%ax, %%ds\n"
                             "mov %%ax, %%es\n"
                             "mov %%ax, %%fs\n"
                             "mov %%ax, %%gs\n"
                             "mov %%ax, %%ss\n"
                             : : "m"(gp) : "eax", "memory"
                         );
                     }
