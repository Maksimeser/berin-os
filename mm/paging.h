#ifndef BERIN_PAGING_H
#define BERIN_PAGING_H

#include <stdint.h>

#define PTE_PRESENT  0x01
#define PTE_WRITABLE 0x02
#define PTE_USER     0x04

void     paging_init(uint32_t mb_info_addr);
uint32_t paging_pde_count(void);
uint32_t paging_get_cr3(void);
uint32_t paging_get_cr2(void);
int      paging_enabled(void);

#endif
