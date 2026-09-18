#ifndef BERIN_PMM_H
#define BERIN_PMM_H

#include <stdint.h>

#define PAGE_SIZE  4096
#define PMM_MAX_MEM (4u * 1024u * 1024u * 1024u)
#define PMM_MAX_PAGES (PMM_MAX_MEM / PAGE_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_PAGES / 8)

void     pmm_init(uint32_t mb_info_addr);
void    *pmm_alloc_page(void);
void     pmm_free_page(void *page);
uint32_t pmm_total_pages(void);
uint32_t pmm_free_pages(void);
uint32_t pmm_used_pages(void);

#endif
