/* bput - put a host file into a BerFS image.
 * Usage: bput <image> <host_file> [name_in_fs]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "berfs_host.h"
#include "berfs_dir.h"
#include "berfs_alloc.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: bput <image> <host_file> [name_in_fs]\n");
        return 1;
    }

    const char *img       = argv[1];
    const char *host_path = argv[2];
    const char *name      = (argc >= 4) ? argv[3] : NULL;

    if (!name) {
        /* derive from host path basename */
        const char *p = strrchr(host_path, '/');
        name = p ? p + 1 : host_path;
    }

    FILE *hf = fopen(host_path, "rb");
    if (!hf) {
        fprintf(stderr, "[ FAIL ] not_found: %s\n", host_path);
        return 1;
    }
    fseek(hf, 0, SEEK_END);
    long hsize = ftell(hf);
    fseek(hf, 0, SEEK_SET);
    if (hsize < 0) { fclose(hf); fprintf(stderr, "[ FAIL ] ftell\n"); return 1; }

    if ((size_t)hsize > (size_t)BERFS_DIRECT_BLOCKS * BERFS_BLOCK_SIZE) {
        fprintf(stderr, "[ FAIL ] no_space: file too large for BerFS v1 (%ld bytes, max %u)\n",
                hsize, BERFS_DIRECT_BLOCKS * BERFS_BLOCK_SIZE);
        fclose(hf);
        return 1;
    }

    void *data = malloc((size_t)hsize);
    if (!data) { fclose(hf); fprintf(stderr, "[ FAIL ] out_of_memory\n"); return 1; }
    if (fread(data, 1, (size_t)hsize, hf) != (size_t)hsize) {
        fprintf(stderr, "[ FAIL ] read error\n");
        free(data); fclose(hf);
        return 1;
    }
    fclose(hf);

    struct berfs_fs fs;
    if (berfs_open(&fs, img, 0) != 0) {
        fprintf(stderr, "[ FAIL ] cannot open image %s\n", img);
        free(data);
        return 1;
    }

    /* check name not already present */
    struct berfs_inode root;
    berfs_read_inode(&fs, BERFS_ROOT_INODE, &root);
    if (berfs_dir_lookup(&fs, &root, name) != 0) {
        fprintf(stderr, "[ FAIL ] already_exists: %s\n", name);
        berfs_close(&fs);
        free(data);
        return 1;
    }

    /* allocate inode */
    uint32_t ino = berfs_inode_alloc(&fs);
    if (ino == 0) {
        fprintf(stderr, "[ FAIL ] no_space: no free inode\n");
        berfs_close(&fs);
        free(data);
        return 1;
    }

    /* build file inode */
    struct berfs_inode fi;
    memset(&fi, 0, sizeof(fi));
    fi.i_mode        = BERFS_TYPE_FILE | 0644;
    fi.i_uid         = 0;
    fi.i_size        = 0;
    fi.i_links_count = 1;
    fi.i_blocks      = 0;

    /* write data — this allocates blocks and updates fi */
    if ((size_t)hsize > 0) {
        if (berfs_file_write(&fs, &fi, 0, data, (uint32_t)hsize) != 0) {
            fprintf(stderr, "[ FAIL ] no_space: cannot write data\n");
            berfs_inode_free(&fs, ino);
            berfs_close(&fs);
            free(data);
            return 1;
        }
    }

    if (berfs_write_inode(&fs, ino, &fi) != 0) {
        fprintf(stderr, "[ FAIL ] cannot write inode\n");
        berfs_inode_free(&fs, ino);
        berfs_close(&fs);
        free(data);
        return 1;
    }

    /* ---- ПАРСИМ ПУТЬ ---- */
    char dir_path[256] = "/";
    char file_name[256];

    const char *last_slash = strrchr(name, '/');
    if (last_slash) {
        int dlen = last_slash - name;
        if (dlen == 0) {
            strcpy(dir_path, "/");
        } else {
            memcpy(dir_path, name, dlen);
            dir_path[dlen] = '\0';
        }
        strcpy(file_name, last_slash + 1);
    } else {
        strcpy(dir_path, "/");
        strcpy(file_name, name);
    }

    /* Находим родительскую директорию */
    struct berfs_inode parent;
    uint32_t parent_ino = BERFS_ROOT_INODE;

    if (strcmp(dir_path, "/") != 0) {
        // Ищем папку в корне
        parent_ino = berfs_dir_lookup(&fs, &root, dir_path + 1); // пропускаем первый '/'
        if (parent_ino == 0) {
            fprintf(stderr, "[ FAIL ] no such directory: %s\n", dir_path);
            berfs_inode_free(&fs, ino);
            berfs_close(&fs);
            free(data);
            return 1;
        }
        if (berfs_read_inode(&fs, parent_ino, &parent) != 0) {
            fprintf(stderr, "[ FAIL ] cannot read parent inode\n");
            berfs_inode_free(&fs, ino);
            berfs_close(&fs);
            free(data);
            return 1;
        }
    } else {
        parent = root;
    }

    /* check name not already present */
    if (berfs_dir_lookup(&fs, &parent, file_name) != 0) {
        fprintf(stderr, "[ FAIL ] already_exists: %s\n", file_name);
        berfs_close(&fs);
        free(data);
        return 1;
    }

    /* add dirent to parent dir */
    if (berfs_dir_add(&fs, &parent, parent_ino, file_name, ino, BERFS_DT_FILE) != 0) {
        fprintf(stderr, "[ FAIL ] cannot add dir entry to %s\n", dir_path);
        berfs_inode_free(&fs, ino);
        berfs_close(&fs);
        free(data);
        return 1;
    }

    printf("[ OK ] put %s -> %s (%ld bytes, inode %u)\n", host_path, name, hsize, ino);

    printf("[ OK ] put %s -> %s (%ld bytes, inode %u)\n", host_path, name, hsize, ino);

    berfs_close(&fs);
    free(data);
    return 0;
}
