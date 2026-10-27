/* brm - remove a file from BerFS image. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "berfs_host.h"
#include "berfs_dir.h"
#include "berfs_alloc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: brm <image> <name>\n");
        return 1;
    }

    struct berfs_fs fs;
    if (berfs_open(&fs, argv[1], 0) != 0) {
        fprintf(stderr, "[ FAIL ] cannot open %s\n", argv[1]);
        return 1;
    }

    struct berfs_inode root;
    if (berfs_read_inode(&fs, BERFS_ROOT_INODE, &root) != 0) {
        fprintf(stderr, "[ FAIL ] cannot read root\n");
        berfs_close(&fs);
        return 1;
    }

    // Ищем файл
    uint32_t ino = berfs_dir_lookup(&fs, &root, argv[2]);
    if (ino == 0) {
        fprintf(stderr, "[ FAIL ] not_found: %s\n", argv[2]);
        berfs_close(&fs);
        return 1;
    }

    // Освобождаем блоки данных и сам инод
    struct berfs_inode fi;
    if (berfs_read_inode(&fs, ino, &fi) == 0) {
        for (int i = 0; i < BERFS_DIRECT_BLOCKS; i++) {
            if (fi.i_block[i] != 0) {
                berfs_block_free(&fs, fi.i_block[i]);
            }
        }
        berfs_inode_free(&fs, ino);
    }

    // Теперь зануляем запись в директории (d_inode = 0)
    // Для простоты работаем в памяти и перезаписываем весь блок
    for (int blk = 0; blk < BERFS_DIRECT_BLOCKS; blk++) {
        if (root.i_block[blk] == 0) break;

        uint8_t buf[BERFS_BLOCK_SIZE];
        if (berfs_read_block(&fs, root.i_block[blk], buf) != 0) break;

        uint32_t pos = 0;
        int found = 0;
        while (pos < BERFS_BLOCK_SIZE) {
            struct berfs_dirent *de = (struct berfs_dirent *)(buf + pos);
            if (de->d_reclen == 0) break; // Конец блока

            if (de->d_inode != 0 && de->d_name_len > 0) {
                char name[BERFS_NAME_MAX + 1];
                memcpy(name, buf + pos + sizeof(struct berfs_dirent), de->d_name_len);
                name[de->d_name_len] = '\0';

                if (strcmp(name, argv[2]) == 0) {
                    // ВОТ ОНО: зануляем inode, чтобы запись исчезла
                    de->d_inode = 0;
                    found = 1;
                    break;
                }
            }
            pos += de->d_reclen;
        }

        if (found) {
            berfs_write_block(&fs, root.i_block[blk], buf);
            break;
        }
    }

    printf("[ OK ] removed %s (inode %u)\n", argv[2], ino);
    berfs_close(&fs);
    return 0;
}
