/* Berin OS - kernel heap (Phase 3.3)
 * Bump allocator with free-list and coalescing.
 * Region: 1 MB starting right after kernel end (identity-mapped).
 */

#include "heap.h"

extern uint8_t _kernel_end[];

#define HEAP_SIZE       (1024u * 1024u)   /* 1 MB */
#define ALIGN_UP(x, a)  (((x) + ((a) - 1)) & ~((a) - 1))
#define CHUNK_MAGIC     0xBEEF1234u

struct chunk {
    uint32_t      magic;
    uint32_t      size;      /* payload size in bytes */
    int           free;
    struct chunk *next;
    struct chunk *prev;
};

static struct chunk *head;
static uint32_t      used_bytes;
static uint32_t      total_bytes;

void heap_init(void) {
    uint32_t start = (uint32_t)_kernel_end;
    start = ALIGN_UP(start, 16);

    head = (struct chunk *)start;
    head->magic = CHUNK_MAGIC;
    head->size  = HEAP_SIZE - sizeof(struct chunk);
    head->free  = 1;
    head->next  = NULL;
    head->prev  = NULL;

    total_bytes = HEAP_SIZE;
    used_bytes  = 0;
}

static void split(struct chunk *c, uint32_t want) {
    if (c->size < want + sizeof(struct chunk) + 16) return;
    struct chunk *n = (struct chunk *)((uint8_t *)c + sizeof(struct chunk) + want);
    n->magic = CHUNK_MAGIC;
    n->size  = c->size - want - sizeof(struct chunk);
    n->free  = 1;
    n->next  = c->next;
    n->prev  = c;
    if (n->next) n->next->prev = n;
    c->next  = n;
    c->size  = want;
}

static void coalesce(struct chunk *c) {
    if (c->next && c->next->free) {
        struct chunk *n = c->next;
        c->size += sizeof(struct chunk) + n->size;
        c->next  = n->next;
        if (c->next) c->next->prev = c;
    }
    if (c->prev && c->prev->free) {
        coalesce(c->prev);
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    uint32_t want = ALIGN_UP((uint32_t)size, 16);

    for (struct chunk *c = head; c; c = c->next) {
        if (!c->free || c->size < want) continue;
        split(c, want);
        c->free = 0;
        used_bytes += c->size;
        return (void *)((uint8_t *)c + sizeof(struct chunk));
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    struct chunk *c = (struct chunk *)((uint8_t *)ptr - sizeof(struct chunk));
    if (c->magic != CHUNK_MAGIC) return;
    if (c->free) return;
    c->free = 1;
    used_bytes -= c->size;
    coalesce(c);
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    struct chunk *c = (struct chunk *)((uint8_t *)ptr - sizeof(struct chunk));
    if (c->magic != CHUNK_MAGIC) return NULL;

    uint32_t want = ALIGN_UP((uint32_t)new_size, 16);
    if (c->size >= want) return ptr;

    void *np = kmalloc(new_size);
    if (!np) return NULL;

    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)np;
    for (uint32_t i = 0; i < c->size; i++) dst[i] = src[i];
    kfree(ptr);
    return np;
}

uint32_t heap_total(void) { return total_bytes; }
uint32_t heap_used(void)  { return used_bytes;  }
uint32_t heap_free(void)  { return total_bytes - used_bytes; }
