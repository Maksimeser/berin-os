#include "vmm.h"
#include "pmm.h"
#include "kstring.h"

/* Получаем текущую PML4 из CR3 */
static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & ~0xFFF;
}

void vmm_switch_pml4(uint64_t *pml4) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pml4));
}

uint64_t *vmm_create_pml4(void) {
    // 1. Выделяем новый PML4
    uint64_t *new_pml4 = (uint64_t *)pmm_alloc_page();
    if (!new_pml4) return 0;

    // 2. Очищаем (на всякий случай, pmm_alloc не всегда чистит)
    memset(new_pml4, 0, PAGE_SIZE);

    // 3. Берем текущую PML4 (у ядра identity-mapped)
    uint64_t *old_pml4 = (uint64_t *)read_cr3();

    // 4. Копируем ЯДЕРНЫЕ записи (PML4 entries 256..511)
    // В x86_64 нижняя половина (0..255) для userland, верхняя (256..511) для ядра.
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }

    // *Примечание*: Сейчас у нас identity-map (0..128MB).
    // Записи ядра лежат в первых PML4-записях (0..0).
    // Для чистоты в будущем нужно будет перенести ядро в upper-half (0xFFFFFFFF...).
    // Но пока оставим как есть — скопируем ВСЕ записи.
    for (int i = 0; i < 256; i++) {
        new_pml4[i] = old_pml4[i];
    }

    return new_pml4;
}

/* Walk: спускаемся по таблицам */
uint64_t vmm_get_phys(uint64_t *pml4, uint64_t vaddr) {
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    uint64_t idx_pd   = (vaddr >> 21) & 0x1FF;
    uint64_t idx_pt   = (vaddr >> 12) & 0x1FF;

    if (!(pml4[idx_pml4] & PTE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t *)(pml4[idx_pml4] & ~0xFFF);

    if (!(pdpt[idx_pdpt] & PTE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t *)(pdpt[idx_pdpt] & ~0xFFF);

    if (!(pd[idx_pd] & PTE_PRESENT)) return 0;

    // Проверка на 2MB huge page (бит PS = 7)
    if (pd[idx_pd] & (1ULL << 7)) {
        return (pd[idx_pd] & ~0x1FFFFF) + (vaddr & 0x1FFFFF);
    }

    uint64_t *pt = (uint64_t *)(pd[idx_pd] & ~0xFFF);
    if (!(pt[idx_pt] & PTE_PRESENT)) return 0;

    return (pt[idx_pt] & ~0xFFF) + (vaddr & 0xFFF);
}

int vmm_map_page(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    uint64_t idx_pd   = (vaddr >> 21) & 0x1FF;
    uint64_t idx_pt   = (vaddr >> 12) & 0x1FF;

    // PML4 -> PDPT
    if (!(pml4[idx_pml4] & PTE_PRESENT)) {
        uint64_t *new_pdpt = (uint64_t *)pmm_alloc_page();
        if (!new_pdpt) return -1;
        memset(new_pdpt, 0, PAGE_SIZE);
        pml4[idx_pml4] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    uint64_t *pdpt = (uint64_t *)(pml4[idx_pml4] & ~0xFFF);

    // PDPT -> PD
    if (!(pdpt[idx_pdpt] & PTE_PRESENT)) {
        uint64_t *new_pd = (uint64_t *)pmm_alloc_page();
        if (!new_pd) return -1;
        memset(new_pd, 0, PAGE_SIZE);
        pdpt[idx_pdpt] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    uint64_t *pd = (uint64_t *)(pdpt[idx_pdpt] & ~0xFFF);

    // PD -> PT
    if (!(pd[idx_pd] & PTE_PRESENT)) {
        uint64_t *new_pt = (uint64_t *)pmm_alloc_page();
        if (!new_pt) return -1;
        memset(new_pt, 0, PAGE_SIZE);
        pd[idx_pd] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else if (pd[idx_pd] & (1ULL << 7)) {
        uint64_t *new_pt = (uint64_t *)pmm_alloc_page();
        if (!new_pt) return -1;
        uint64_t huge_phys = pd[idx_pd] & ~0x1FFFFFULL;
        uint64_t old_flags = pd[idx_pd] & 0xFFF;

        for (int i = 0; i < 512; i++) {
            new_pt[i] = (huge_phys + i * PAGE_SIZE) | old_flags;
        }

        pd[idx_pd] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    uint64_t *pt = (uint64_t *)(pd[idx_pd] & ~0xFFF);

    pt[idx_pt] = paddr | flags;
    __asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}
