#ifndef BERFS_HOST_H
#define BERFS_HOST_H

/* Host-side BerFS library (used by bformat, bfsck, bls, bput, bget).
 * Works on a regular file (disk.img), not on block devices.
 */

#include <stdint.h>
#include <stdio.h>
#include "../include/berfs.h"

struct berfs_fs {
    FILE *fp;
    uint64_t base_offset;      /* offset in file where BerFS starts (for --offset) */
    struct berfs_superblock sb;
};

/* Open image and load superblock. Returns 0 on success. */
int  berfs_open(struct berfs_fs *fs, const char *path, uint64_t base_offset);

/* Close image. */
void berfs_close(struct berfs_fs *fs);

/* Read/write one block (BLOCK_SIZE bytes) at block index. */
int  berfs_read_block (struct berfs_fs *fs, uint32_t block, void *buf);
int  berfs_write_block(struct berfs_fs *fs, uint32_t block, const void *buf);

/* Read/write one inode by inode number (1-based). */
int  berfs_read_inode (struct berfs_fs *fs, uint32_t ino, struct berfs_inode *out);
int  berfs_write_inode(struct berfs_fs *fs, uint32_t ino, const struct berfs_inode *in);

/* Read/write inode bitmap / block bitmap (whole bitmap blocks). */
int  berfs_read_inode_bitmap (struct berfs_fs *fs, uint8_t *buf);
int  berfs_read_block_bitmap (struct berfs_fs *fs, uint8_t *buf);

/* Write superblock back to disk. */
int  berfs_write_superblock(struct berfs_fs *fs);

/* Bitmap helpers (operate on raw bitmap memory, not on disk). */
static inline int berfs_bit_test(const uint8_t *bm, uint32_t bit) {
    return (bm[bit >> 3] >> (bit & 7)) & 1;
}
static inline void berfs_bit_set(uint8_t *bm, uint32_t bit) {
    bm[bit >> 3] |= (uint8_t)(1u << (bit & 7));
}
static inline void berfs_bit_clear(uint8_t *bm, uint32_t bit) {
    bm[bit >> 3] &= (uint8_t)~(1u << (bit & 7));
}

#endif
