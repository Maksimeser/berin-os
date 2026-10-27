/* Berin - kernel heap: linear bump allocator (без free) */
#include "heap.h"
extern uint8_t _kernel_end[];

#define HEAP_SIZE (4ULL * 1024ULL * 1024ULL)   /* 4 MB */
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

static uint8_t *heap_top;
static uint8_t *heap_end;
static uint64_t used_bytes;

void heap_init(void) {
    uint8_t *base = (uint8_t *)ALIGN_UP((uint64_t)_kernel_end, 16);
    heap_top = base;
    heap_end = base + HEAP_SIZE;
    used_bytes = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;
    uint64_t want = ALIGN_UP((uint64_t)size, 16);
    if (heap_top + want > heap_end) return 0;
    void *p = heap_top;
    heap_top += want;
    used_bytes += want;
    return p;
}

void kfree(void *ptr) { (void)ptr; /* no-op */ }

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return 0; }
    void *np = kmalloc(new_size);
    if (!np) return 0;
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)np;
    for (size_t i = 0; i < new_size; i++) dst[i] = src[i];
    return np;
}

uint64_t heap_total(void) { return HEAP_SIZE; }
uint64_t heap_used(void)  { return used_bytes; }
uint64_t heap_free(void)  { return HEAP_SIZE - used_bytes; }
