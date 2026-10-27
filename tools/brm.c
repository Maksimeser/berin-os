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
        fprintf(stderr, "usage: brm <image> <path>\n");
        return 1;
    }

    struct berfs_fs fs;
    if (berfs_open(&fs, argv[1], 0) != 0) {
        fprintf(stderr, "[ FAIL ] cannot open %s\n", argv[1]);
        return 1;
    }

    const char *path = argv[2];

    /* Разбираем путь: ищем последний слэш */
    char parent_path[256] = "/";
    char file_name[256];

    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        int dlen = last_slash - path;
        if (dlen > 0) {
            memcpy(parent_path, path, dlen);
            parent_path[dlen] = '\0';
        }
        strcpy(file_name, last_slash + 1);
    } else {
        strcpy(file_name, path);
    }

    /* Всегда сначала читаем ROOT inode */
    struct berfs_inode parent;
    uint32_t parent_ino = BERFS_ROOT_INODE;

    if (berfs_read_inode(&fs, BERFS_ROOT_INODE, &parent) != 0) {
        fprintf(stderr, "[ FAIL ] cannot read root inode\n");
        berfs_close(&fs);
        return 1;
    }

    /* Если путь не корень, ищем папку в корне */
    if (strcmp(parent_path, "/") != 0) {
        uint32_t bin_ino = berfs_dir_lookup(&fs, &parent, parent_path + 1);
        if (bin_ino == 0) {
            fprintf(stderr, "[ FAIL ] no such directory: %s\n", parent_path);
            berfs_close(&fs);
            return 1;
        }
        parent_ino = bin_ino;

        /* Читаем найденную папку */
        if (berfs_read_inode(&fs, parent_ino, &parent) != 0) {
            fprintf(stderr, "[ FAIL ] cannot read parent inode\n");
            berfs_close(&fs);
            return 1;
        }
    }

    /* Ищем файл в этой директории */
    uint32_t ino = berfs_dir_lookup(&fs, &parent, file_name);
    if (ino == 0) {
        fprintf(stderr, "[ FAIL ] not_found: %s\n", path);
        berfs_close(&fs);
        return 1;
    }

    /* Освобождаем блоки и сам инод */
    struct berfs_inode fi;
    if (berfs_read_inode(&fs, ino, &fi) == 0) {
        for (int i = 0; i < BERFS_DIRECT_BLOCKS; i++) {
            if (fi.i_block[i] != 0) {
                berfs_block_free(&fs, fi.i_block[i]);
            }
        }
        berfs_inode_free(&fs, ino);
    }

    /* Зануляем запись в директории вручную (d_inode = 0) */
    for (int blk = 0; blk < BERFS_DIRECT_BLOCKS; blk++) {
        if (parent.i_block[blk] == 0) break;

        uint8_t buf[BERFS_BLOCK_SIZE];
        if (berfs_read_block(&fs, parent.i_block[blk], buf) != 0) break;

        uint32_t pos = 0;
        int found = 0;
        while (pos < BERFS_BLOCK_SIZE) {
            struct berfs_dirent *de = (struct berfs_dirent *)(buf + pos);
            if (de->d_reclen == 0) break;

            if (de->d_inode != 0 && de->d_name_len > 0) {
                char name[BERFS_NAME_MAX + 1];
                memcpy(name, buf + pos + sizeof(struct berfs_dirent), de->d_name_len);
                name[de->d_name_len] = '\0';

                if (strcmp(name, file_name) == 0) {
                    de->d_inode = 0;
                    found = 1;
                    break;
                }
            }
            pos += de->d_reclen;
        }

        if (found) {
            berfs_write_block(&fs, parent.i_block[blk], buf);
            break;
        }
    }

    printf("[ OK ] removed %s (inode %u)\n", path, ino);
    berfs_close(&fs);
    return 0;
}
