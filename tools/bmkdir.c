#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "berfs_host.h"
#include "berfs_dir.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: bmkdir <image> <path>\n");
        return 1;
    }

    berfs_t fs;
    if (berfs_mount(argv[1], &fs) != 0) {
        printf("[ FAIL ] could not mount image\n");
        return 1;
    }

    // В berfs_dir.c должна быть эта функция
    int res = berfs_mkdir(&fs, argv[2]);

    if (res == 0) {
        printf("[ OK ] created directory: %s\n", argv[2]);
    } else {
        printf("[ FAIL ] could not create directory (exists?)\n");
    }

    return 0;
}
