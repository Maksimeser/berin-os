/* Berin OS - 2-level paging (i386)
 * Phase 3.2a: identity-map first 4 MB (virt == phys), then enable CR0.PG.
 */

#include "paging.h"

/* Both structures are 4K-aligned and live in .bss, so pmm_init()
 * (which starts marking pages free above _kernel_end) already
 * treats them as reserved. */

static uint32_t page_directory[1024]   __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));

void paging_init(void) {
    /* 1. Fill first page table: identity-map 1024 pages = 4 MB */
    for (uint32_t i = 0; i < 1024; i++)
        first_page_table[i] = (i * 4096u) | PTE_PRESENT | PTE_WRITABLE;

    /* 2. Clear page directory, then PDE[0] -> first_page_table */
    for (uint32_t i = 0; i < 1024; i++)
        page_directory[i] = 0;

    page_directory[0] = ((uint32_t)first_page_table)
    | PTE_PRESENT | PTE_WRITABLE;

    /* 3. Point CR3 at page directory */
    __asm__ __volatile__("mov %0, %%cr3" :: "r"((uint32_t)page_directory));

    /* 4. Enable paging: CR0.PG = bit 31 */
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
}

uint32_t paging_get_cr3(void) {
    uint32_t v;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(v));
    return v;
}

uint32_t paging_get_cr2(void) {
    uint32_t v;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(v));
    return v;
}

int paging_enabled(void) {
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000u) ? 1 : 0;
}
