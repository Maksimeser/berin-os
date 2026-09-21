#ifndef BERIN_HEAP_H
#define BERIN_HEAP_H

#include <stdint.h>
#include <stddef.h>

void  heap_init(void);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);

uint32_t heap_total(void);
uint32_t heap_used(void);
uint32_t heap_free(void);

#endif
