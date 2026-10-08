#include "berfs_host.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int read_at(FILE *fp, uint64_t offset, void *buf, size_t len) {
    if (fseek(fp, (long)offset, SEEK_SET) != 0) return -1;
    if (fread(buf, 1, len, fp) != len) return -1;
    return 0;
}

static int write_at(FILE *fp, uint64_t offset, const void *buf, size_t len) {
    if (fseek(fp, (long)offset, SEEK_SET) != 0) return -1;
    if (fwrite(buf, 1, len, fp) != len) return -1;
    return 0;
}

int berfs_open(struct berfs_fs *fs, const char *path, uint64_t base_offset) {
    memset(fs, 0, sizeof(*fs));
    fs->base_offset = base_offset;
    fs->fp = fopen(path, "rb+");
    if (!fs->fp) return -1;

    if (read_at(fs->fp, base_offset + BERFS_BLOCK_SIZE, &fs->sb, sizeof(fs->sb)) != 0) {
        fclose(fs->fp);
        fs->fp = 0;
        return -2;
    }
    if (fs->sb.sb_magic != BERFS_MAGIC) {
        fclose(fs->fp);
        fs->fp = 0;
        return -3;
    }
    return 0;
}

void berfs_close(struct berfs_fs *fs) {
    if (fs->fp) fclose(fs->fp);
    fs->fp = 0;
}

int berfs_read_block(struct berfs_fs *fs, uint32_t block, void *buf) {
    return read_at(fs->fp,
                   fs->base_offset + (uint64_t)block * BERFS_BLOCK_SIZE,
                   buf, BERFS_BLOCK_SIZE);
}

int berfs_write_block(struct berfs_fs *fs, uint32_t block, const void *buf) {
    return write_at(fs->fp,
                    fs->base_offset + (uint64_t)block * BERFS_BLOCK_SIZE,
                    buf, BERFS_BLOCK_SIZE);
}

int berfs_read_inode(struct berfs_fs *fs, uint32_t ino, struct berfs_inode *out) {
    if (ino >= fs->sb.sb_total_inodes) return -1;
    uint32_t inodes_per_block = BERFS_BLOCK_SIZE / sizeof(struct berfs_inode);
    uint32_t block = fs->sb.sb_inode_table_block + (ino / inodes_per_block);
    uint32_t index = ino % inodes_per_block;

    uint8_t buf[BERFS_BLOCK_SIZE];
    if (berfs_read_block(fs, block, buf) != 0) return -2;
    memcpy(out, buf + index * sizeof(struct berfs_inode), sizeof(struct berfs_inode));
    return 0;
}

int berfs_write_inode(struct berfs_fs *fs, uint32_t ino, const struct berfs_inode *in) {
    if (ino >= fs->sb.sb_total_inodes) return -1;
    uint32_t inodes_per_block = BERFS_BLOCK_SIZE / sizeof(struct berfs_inode);
    uint32_t block = fs->sb.sb_inode_table_block + (ino / inodes_per_block);
    uint32_t index = ino % inodes_per_block;

    uint8_t buf[BERFS_BLOCK_SIZE];
    if (berfs_read_block(fs, block, buf) != 0) return -2;
    memcpy(buf + index * sizeof(struct berfs_inode), in, sizeof(struct berfs_inode));
    if (berfs_write_block(fs, block, buf) != 0) return -3;
    return 0;
}

int berfs_read_inode_bitmap(struct berfs_fs *fs, uint8_t *buf) {
    uint32_t nblocks = (fs->sb.sb_total_inodes + 8 * BERFS_BLOCK_SIZE - 1) / (8 * BERFS_BLOCK_SIZE);
    for (uint32_t i = 0; i < nblocks; i++) {
        if (berfs_read_block(fs, fs->sb.sb_inode_bitmap_block + i,
            buf + (size_t)i * BERFS_BLOCK_SIZE) != 0) return -1;
    }
    return 0;
}

int berfs_read_block_bitmap(struct berfs_fs *fs, uint8_t *buf) {
    uint32_t nblocks = (fs->sb.sb_total_blocks + 8 * BERFS_BLOCK_SIZE - 1) / (8 * BERFS_BLOCK_SIZE);
    for (uint32_t i = 0; i < nblocks; i++) {
        if (berfs_read_block(fs, fs->sb.sb_block_bitmap_block + i,
            buf + (size_t)i * BERFS_BLOCK_SIZE) != 0) return -1;
    }
    return 0;
}

int berfs_write_superblock(struct berfs_fs *fs) {
    return write_at(fs->fp, fs->base_offset + BERFS_BLOCK_SIZE, &fs->sb, sizeof(fs->sb));
}
