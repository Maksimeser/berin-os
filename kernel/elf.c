/* Berin - ELF64 loader (Stage 6.1, minimal) */

#include "elf.h"

extern void term_write(const char *s);

/* returns 0 on success */
int elf_load(const uint8_t *data, uint64_t size, uint64_t *entry_out) {
    if (size < sizeof(struct elf64_ehdr)) {
        term_write("[elf] file too small\n");
        return -1;
    }

    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)data;

    if (*(const uint32_t *)eh->e_ident != ELF_MAGIC) {
        term_write("[elf] bad magic\n");
        return -1;
    }
    if (eh->e_ident[4] != ELFCLASS64) {
        term_write("[elf] not ELF64\n");
        return -1;
    }
    if (eh->e_ident[5] != ELFDATA2LSB) {
        term_write("[elf] not little-endian\n");
        return -1;
    }
    if (eh->e_type != ET_EXEC) {
        term_write("[elf] not ET_EXEC\n");
        return -1;
    }
    if (eh->e_machine != EM_X86_64) {
        term_write("[elf] not x86_64\n");
        return -1;
    }

    /* iterate program headers */
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const struct elf64_phdr *ph = (const struct elf64_phdr *)
        (data + eh->e_phoff + (uint64_t)i * eh->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_filesz > ph->p_memsz) {
            term_write("[elf] filesz > memsz\n");
            return -1;
        }

        uint8_t *dst       = (uint8_t *)ph->p_vaddr;
        const uint8_t *src = data + ph->p_offset;

        for (uint64_t k = 0; k < ph->p_filesz; k++) dst[k] = src[k];
        for (uint64_t k = ph->p_filesz; k < ph->p_memsz; k++) dst[k] = 0;
    }

    *entry_out = eh->e_entry;
    return 0;
}
