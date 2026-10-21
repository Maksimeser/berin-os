#include "elf.h"
#include "vmm.h"
#include "pmm.h"
#include "kstring.h"

int elf_load(const uint8_t *data, uint64_t size, uint64_t *pml4, uint64_t *entry_out) {
    (void)pml4; // Пока игнорируем VMM для прямого запуска
    if (size < sizeof(struct elf64_ehdr)) return -1;
    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)data;

    if (*(const uint32_t *)eh->e_ident != ELF_MAGIC) return -1;
    if (eh->e_ident[4] != ELFCLASS64) return -1;
    if (eh->e_ident[5] != ELFDATA2LSB) return -1;
    if (eh->e_type != ET_EXEC) return -1;
    if (eh->e_machine != EM_X86_64) return -1;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const struct elf64_phdr *ph = (const struct elf64_phdr *)(data + eh->e_phoff + (uint64_t)i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_filesz > ph->p_memsz) return -1;

        uint64_t vaddr_start = ph->p_vaddr;
        uint64_t vaddr_end = ph->p_vaddr + ph->p_memsz;

        for (uint64_t vaddr = vaddr_start & ~0xFFFULL; vaddr < vaddr_end; vaddr += 4096) {
            uint64_t phys = vaddr; // Identity map

            uint64_t page_start = vaddr;
            uint64_t page_end = vaddr + 4096;
            uint64_t copy_start = page_start > ph->p_vaddr ? page_start : ph->p_vaddr;
            uint64_t copy_end = page_end < (ph->p_vaddr + ph->p_filesz) ? page_end : (ph->p_vaddr + ph->p_filesz);

            if (copy_start < copy_end) {
                uint64_t len = copy_end - copy_start;
                uint64_t file_offset = ph->p_offset + (copy_start - ph->p_vaddr);
                uint64_t phys_dst = phys + (copy_start - page_start);
                memcpy((void *)phys_dst, data + file_offset, len);
            }
        }
    }
    *entry_out = eh->e_entry;
    return 0;
}
