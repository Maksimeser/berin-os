#ifndef BERIN_I386_GDT_H
#define BERIN_I386_GDT_H

#include <stdint.h>

void gdt_init(void);

/* Segment selectors (offsets into GDT, RPL=0) */
#define SEG_KCODE 0x08
#define SEG_KDATA 0x10

#endif
