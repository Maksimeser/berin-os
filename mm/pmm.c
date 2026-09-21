/* Berin OS - physical memory manager (bitmap, 4K pages)
 * Reads Multiboot1 info to detect RAM above 1 MB.
 */

#include "pmm.h"

extern uint8_t _kernel_end[];    /* from linker.ld */

static uint8_t  bitmap[PMM_BITMAP_BYTES];
static uint32_t total_pages;
static uint32_t free_pages;

static inline void bit_set(uint32_t i)   { bitmap[i >> 3] |=  (uint8_t)(1u << (i & 7)); }
static inline void bit_clear(uint32_t i) { bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); }
static inline int  bit_get(uint32_t i)   { return (bitmap[i >> 3] >> (i & 7)) & 1u; }

/* Multiboot1 info: mem_upper is at offset 8 (KB above 1 MB) */
static uint32_t read_mem_upper_kb(uint32_t mb_info_addr) {
    if (mb_info_addr == 0) return 0;
    const uint32_t *mb = (const uint32_t *)mb_info_addr;
    return mb[2];                /* offset 8 */
}

void pmm_init(uint32_t mb_info_addr) {
    /* mark everything used initially */
    for (uint32_t i = 0; i < PMM_BITMAP_BYTES; i++) bitmap[i] = 0xFF;

    uint32_t mem_upper_kb = read_mem_upper_kb(mb_info_addr);
    uint32_t total_bytes  = 0x100000u + mem_upper_kb * 1024u;
    total_pages = total_bytes / PAGE_SIZE;
    if (total_pages > PMM_MAX_PAGES) total_pages = PMM_MAX_PAGES;

    /* free range from end of kernel to end of RAM */
    uint32_t kend       = (uint32_t)_kernel_end;
    uint32_t start_page = (kend + PAGE_SIZE - 1) / PAGE_SIZE;

    free_pages = 0;
    for (uint32_t p = start_page; p < total_pages; p++) {
        bit_clear(p);
        free_pages++;
    }
}

void *pmm_alloc_page(void) {
    for (uint32_t p = 0; p < total_pages; p++) {
        if (!bit_get(p)) {
            bit_set(p);
            free_pages--;
            return (void *)(p * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void *page) {
    uint32_t addr = (uint32_t)page;
    uint32_t p = addr / PAGE_SIZE;
    if (p >= total_pages) return;
    if (bit_get(p)) {
        bit_clear(p);
        free_pages++;
    }
}

uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_free_pages(void)  { return free_pages; }
uint32_t pmm_used_pages(void)  { return total_pages - free_pages; }
