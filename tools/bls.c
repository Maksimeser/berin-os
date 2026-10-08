/* bls - list BerFS directory contents.
 * Usage: bls <image> [path]
 * For v0.1, path is ignored — always lists root.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "berfs_host.h"
#include "berfs_dir.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: bls <image>\n");
        return 1;
    }

    struct berfs_fs fs;
    if (berfs_open(&fs, argv[1], 0) != 0) {
        fprintf(stderr, "[ FAIL ] cannot open %s\n", argv[1]);
        return 1;
    }

    struct berfs_inode root;
    if (berfs_read_inode(&fs, BERFS_ROOT_INODE, &root) != 0) {
        fprintf(stderr, "[ FAIL ] cannot read root inode\n");
        berfs_close(&fs);
        return 1;
    }

    if ((root.i_mode & 0xF000u) != BERFS_TYPE_DIR) {
        fprintf(stderr, "[ FAIL ] root is not a directory\n");
        berfs_close(&fs);
        return 1;
    }

    printf("root/\n");
    uint32_t pos = 0;
    uint32_t ino;
    char name[BERFS_NAME_MAX + 1];
    uint8_t type;

    int r;
    while ((r = berfs_dir_next(&fs, &root, &pos, &ino, name, &type)) == 1) {
        char typech = (type == BERFS_DT_DIR) ? 'd' : '-';
        printf("  %c  %8u  %s\n", typech, ino, name);
    }

    berfs_close(&fs);
    return 0;
}
