#ifndef BERIN_KERNEL_BERFS_H
#define BERIN_KERNEL_BERFS_H

#include <stdint.h>
#include "berfs.h"

/* in-memory superblock after mount */
struct berfs_mount {
    struct berfs_superblock sb;
    int mounted;
};

/* 0 = ok */
int  berfs_mount_fs(struct berfs_mount *m);
int  berfs_read_inode(struct berfs_mount *m, uint32_t ino, struct berfs_inode *out);
int  berfs_read_block(struct berfs_mount *m, uint32_t block, void *buf);
uint32_t berfs_dir_lookup(struct berfs_mount *m, const struct berfs_inode *dir, const char *name);
int  berfs_file_read(struct berfs_mount *m, const struct berfs_inode *in, uint32_t pos, void *dst, uint32_t len);
/* iterate directory entries */
int  berfs_dir_next(struct berfs_mount *m, const struct berfs_inode *dir,
                    uint32_t *pos, uint32_t *ino, char *name, uint8_t *type);

#endif
