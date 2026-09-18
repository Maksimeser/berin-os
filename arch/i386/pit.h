#ifndef BERIN_I386_PIT_H
#define BERIN_I386_PIT_H

#include <stdint.h>

void pit_init(uint32_t hz);   /* IRQ0 fires at hz per second */

#endif
