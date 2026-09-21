/* Berin OS - 2-level paging (i386)
 * Phase 3.2c: identity-map ALL physical RAM.
 */

#include "paging.h"
#include "pmm.h"

static uint32_t page_directory[1024]   __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));
static uint32_t pde_count_used;

/* Multiboot1: mem_upper at offset 8 (KB above 1 MB) */
static uint32_t read_mem_upper_kb(uint32_t mb_info_addr) {
    if (mb_info_addr == 0) return 0;
    const uint32_t *mb = (const uint32_t *)mb_info_addr;
    return mb[2];
}

static void fill_pt(uint32_t *pt, uint32_t pde_index) {
    for (uint32_t i = 0; i < 1024; i++) {
        uint32_t phys = (pde_index * 1024u + i) * 4096u;
        pt[i] = phys | PTE_PRESENT | PTE_WRITABLE;
    }
}

void paging_init(uint32_t mb_info_addr) {
    /* how many 4-MB chunks do we need? */
    uint32_t mem_upper_kb = read_mem_upper_kb(mb_info_addr);
    uint32_t total_mb     = 1 + mem_upper_kb / 1024;
    uint32_t pde_count    = (total_mb + 3) / 4;
    if (pde_count > 1024) pde_count = 1024;
    if (pde_count < 1)    pde_count = 1;
    pde_count_used = pde_count;

    /* clear directory */
    for (uint32_t i = 0; i < 1024; i++)
        page_directory[i] = 0;

    /* build PDEs + identity-map */
    for (uint32_t pde = 0; pde < pde_count; pde++) {
        uint32_t *pt;
        if (pde == 0) {
            pt = first_page_table;      /* static, always mapped */
        } else {
            pt = (uint32_t *)pmm_alloc_page();
            if (!pt) break;             /* out of memory, stop */
        }
        fill_pt(pt, pde);
        page_directory[pde] = ((uint32_t)pt) | PTE_PRESENT | PTE_WRITABLE;
    }

    /* CR3 = page directory */
    __asm__ __volatile__("mov %0, %%cr3" :: "r"((uint32_t)page_directory));

    /* enable paging */
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
}

uint32_t paging_pde_count(void)   { return pde_count_used; }

uint32_t paging_get_cr3(void) {
    uint32_t v; __asm__ __volatile__("mov %%cr3, %0" : "=r"(v)); return v;
}

uint32_t paging_get_cr2(void) {
    uint32_t v; __asm__ __volatile__("mov %%cr2, %0" : "=r"(v)); return v;
}

int paging_enabled(void) {
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000u) ? 1 : 0;
}
