#include "berfs_alloc.h"
#include <stdlib.h>
#include <string.h>

static int write_inode_bitmap(struct berfs_fs *fs, const uint8_t *bm) {
    uint32_t nblocks = (fs->sb.sb_total_inodes + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    for (uint32_t i = 0; i < nblocks; i++) {
        if (berfs_write_block(fs, fs->sb.sb_inode_bitmap_block + i,
            bm + (size_t)i * BERFS_BLOCK_SIZE) != 0) return -1;
    }
    return 0;
}

static int write_block_bitmap(struct berfs_fs *fs, const uint8_t *bm) {
    uint32_t nblocks = (fs->sb.sb_total_blocks + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    for (uint32_t i = 0; i < nblocks; i++) {
        if (berfs_write_block(fs, fs->sb.sb_block_bitmap_block + i,
            bm + (size_t)i * BERFS_BLOCK_SIZE) != 0) return -1;
    }
    return 0;
}

uint32_t berfs_inode_alloc(struct berfs_fs *fs) {
    uint32_t nblocks = (fs->sb.sb_total_inodes + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    uint8_t *bm = calloc((size_t)nblocks, BERFS_BLOCK_SIZE);
    if (!bm) return 0;
    if (berfs_read_inode_bitmap(fs, bm) != 0) { free(bm); return 0; }

    for (uint32_t i = 1; i < fs->sb.sb_total_inodes; i++) {
        if (!berfs_bit_test(bm, i)) {
            berfs_bit_set(bm, i);
            fs->sb.sb_free_inodes--;
            if (write_inode_bitmap(fs, bm) != 0) {
                fs->sb.sb_free_inodes++;
                free(bm);
                return 0;
            }
            berfs_write_superblock(fs);
            free(bm);
            return i;
        }
    }
    free(bm);
    return 0;
}

int berfs_inode_free(struct berfs_fs *fs, uint32_t ino) {
    if (ino == 0 || ino >= fs->sb.sb_total_inodes) return -1;

    uint32_t nblocks = (fs->sb.sb_total_inodes + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    uint8_t *bm = calloc((size_t)nblocks, BERFS_BLOCK_SIZE);
    if (!bm) return -1;
    if (berfs_read_inode_bitmap(fs, bm) != 0) { free(bm); return -1; }

    if (berfs_bit_test(bm, ino)) {
        berfs_bit_clear(bm, ino);
        fs->sb.sb_free_inodes++;
        if (write_inode_bitmap(fs, bm) != 0) {
            fs->sb.sb_free_inodes--;
            free(bm);
            return -1;
        }
        berfs_write_superblock(fs);
    }
    free(bm);
    return 0;
}

uint32_t berfs_block_alloc(struct berfs_fs *fs) {
    uint32_t nblocks = (fs->sb.sb_total_blocks + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    uint8_t *bm = calloc((size_t)nblocks, BERFS_BLOCK_SIZE);
    if (!bm) return 0;
    if (berfs_read_block_bitmap(fs, bm) != 0) { free(bm); return 0; }

    for (uint32_t i = fs->sb.sb_data_block; i < fs->sb.sb_total_blocks; i++) {
        if (!berfs_bit_test(bm, i)) {
            berfs_bit_set(bm, i);
            fs->sb.sb_free_blocks--;
            if (write_block_bitmap(fs, bm) != 0) {
                fs->sb.sb_free_blocks++;
                free(bm);
                return 0;
            }
            berfs_write_superblock(fs);
            free(bm);
            return i;
        }
    }
    free(bm);
    return 0;
}

int berfs_block_free(struct berfs_fs *fs, uint32_t block) {
    if (block < fs->sb.sb_data_block || block >= fs->sb.sb_total_blocks) return -1;

    uint32_t nblocks = (fs->sb.sb_total_blocks + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    uint8_t *bm = calloc((size_t)nblocks, BERFS_BLOCK_SIZE);
    if (!bm) return -1;
    if (berfs_read_block_bitmap(fs, bm) != 0) { free(bm); return -1; }

    if (berfs_bit_test(bm, block)) {
        berfs_bit_clear(bm, block);
        fs->sb.sb_free_blocks++;
        if (write_block_bitmap(fs, bm) != 0) {
            fs->sb.sb_free_blocks--;
            free(bm);
            return -1;
        }
        berfs_write_superblock(fs);
    }
    free(bm);
    return 0;
}
