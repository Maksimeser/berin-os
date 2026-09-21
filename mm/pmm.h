#ifndef BERIN_PMM_H
#define BERIN_PMM_H

#include <stdint.h>

#define PAGE_SIZE  4096
#define PMM_MAX_PAGES (1u << 20)                     /* 1 048 576 pages = 4 GB */
#define PMM_BITMAP_BYTES (PMM_MAX_PAGES / 8)         /* 131 072 bytes = 128 KB */

void     pmm_init(uint32_t mb_info_addr);
void    *pmm_alloc_page(void);
void     pmm_free_page(void *page);
uint32_t pmm_total_pages(void);
uint32_t pmm_free_pages(void);
uint32_t pmm_used_pages(void);

#endif
