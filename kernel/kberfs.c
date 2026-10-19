#include "kberfs.h"
#include "blockdev.h"
#include "kstring.h"

static void berfs_write_sb(struct berfs_mount *m) {
    uint8_t buf[BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &m->sb, sizeof(m->sb));
    bdev_write(1, buf);
}

int berfs_read_block(struct berfs_mount *m, uint32_t block, void *buf) {
    (void)m;
    return bdev_read(block, buf);
}

int berfs_mount_fs(struct berfs_mount *m) {
    uint8_t buf[BLOCK_SIZE];
    if (berfs_read_block(m, 1, buf) != 0) return -1;
    memcpy(&m->sb, buf, sizeof(m->sb));
    if (m->sb.sb_magic != BERFS_MAGIC) return -2;
    if (m->sb.sb_version != BERFS_VERSION) return -3;
    m->mounted = 1;
    return 0;
}

int berfs_read_inode(struct berfs_mount *m, uint32_t ino, struct berfs_inode *out) {
    if (ino >= m->sb.sb_total_inodes) return -1;
    uint32_t ipb = BLOCK_SIZE / sizeof(struct berfs_inode);
    uint32_t block = m->sb.sb_inode_table_block + (ino / ipb);
    uint32_t index = ino % ipb;

    uint8_t buf[BLOCK_SIZE];
    if (berfs_read_block(m, block, buf) != 0) return -2;
    memcpy(out, buf + index * sizeof(struct berfs_inode), sizeof(struct berfs_inode));
    return 0;
}

int berfs_write_inode(struct berfs_mount *m, uint32_t ino, const struct berfs_inode *in) {
    if (ino >= m->sb.sb_total_inodes) return -1;
    uint32_t ipb = BLOCK_SIZE / sizeof(struct berfs_inode);
    uint32_t block = m->sb.sb_inode_table_block + (ino / ipb);
    uint32_t index = ino % ipb;

    uint8_t buf[BLOCK_SIZE];
    if (berfs_read_block(m, block, buf) != 0) return -2;
    memcpy(buf + index * sizeof(struct berfs_inode), in, sizeof(struct berfs_inode));
    return bdev_write(block, buf);
}

uint32_t berfs_alloc_block(struct berfs_mount *m) {
    uint8_t buf[BLOCK_SIZE];
    uint32_t bitmap_start = m->sb.sb_block_bitmap_block;
    uint32_t bitmap_blocks = (m->sb.sb_total_blocks + 8*BLOCK_SIZE - 1) / (8*BLOCK_SIZE);

    for (uint32_t b = 0; b < bitmap_blocks; b++) {
        berfs_read_block(m, bitmap_start + b, buf);
        for (int i = 0; i < BLOCK_SIZE; i++) {
            if (buf[i] != 0xFF) {
                for (int bit = 0; bit < 8; bit++) {
                    if (!(buf[i] & (1 << bit))) {
                        uint32_t blk = b * BLOCK_SIZE * 8 + i * 8 + bit;
                        if (blk < m->sb.sb_data_block) continue;
                        if (blk >= m->sb.sb_total_blocks) return 0;

                        buf[i] |= (1 << bit);
                        bdev_write(bitmap_start + b, buf);

                        m->sb.sb_free_blocks--;
                        berfs_write_sb(m);
                        return blk;
                    }
                }
            }
        }
    }
    return 0;
}

uint32_t berfs_alloc_inode(struct berfs_mount *m) {
    uint8_t buf[BLOCK_SIZE];
    uint32_t bitmap_start = m->sb.sb_inode_bitmap_block;

    berfs_read_block(m, bitmap_start, buf);
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (buf[i] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!(buf[i] & (1 << bit))) {
                    uint32_t ino = i * 8 + bit;
                    if (ino < 2 || ino >= m->sb.sb_total_inodes) continue;

                    buf[i] |= (1 << bit);
                    bdev_write(bitmap_start, buf);

                    m->sb.sb_free_inodes--;
                    berfs_write_sb(m);
                    return ino;
                }
            }
        }
    }
    return 0;
}

static int file_read_bytes(struct berfs_mount *m, const struct berfs_inode *in,
                           uint32_t pos, void *dst, uint32_t len) {
    uint8_t *p = (uint8_t *)dst;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t bp = pos + i;
        if (bp >= in->i_size) return -1;
        uint32_t bi = bp / BLOCK_SIZE;
        uint32_t off = bp % BLOCK_SIZE;
        if (bi >= BERFS_DIRECT_BLOCKS) return -1;
        if (in->i_block[bi] == 0) return -1;
        uint8_t buf[BLOCK_SIZE];
        if (berfs_read_block(m, in->i_block[bi], buf) != 0) return -1;
        p[i] = buf[off];
    }
    return 0;
                           }

                           int berfs_file_read(struct berfs_mount *m, const struct berfs_inode *in,
                                               uint32_t pos, void *dst, uint32_t len) {
                               return file_read_bytes(m, in, pos, dst, len);
                                               }

                                               int berfs_file_write(struct berfs_mount *m, struct berfs_inode *in,
                                                                    uint32_t pos, const void *src, uint32_t len) {
                                                   const uint8_t *p = (const uint8_t *)src;
                                                   uint32_t done = 0;
                                                   while (done < len) {
                                                       uint32_t bp = pos + done;
                                                       uint32_t blk_idx = bp / BLOCK_SIZE;
                                                       uint32_t off = bp % BLOCK_SIZE;
                                                       if (blk_idx >= BERFS_DIRECT_BLOCKS) return -1;

                                                       uint32_t chunk = BLOCK_SIZE - off;
                                                       if (chunk > len - done) chunk = len - done;

                                                       if (in->i_block[blk_idx] == 0) {
                                                           uint32_t new_blk = berfs_alloc_block(m);
                                                           if (new_blk == 0) return -1;
                                                           in->i_block[blk_idx] = new_blk;
                                                           uint8_t zero[BLOCK_SIZE];
                                                           memset(zero, 0, sizeof(zero));
                                                           bdev_write(new_blk, zero);
                                                       }

                                                       uint8_t buf[BLOCK_SIZE];
                                                       bdev_read(in->i_block[blk_idx], buf);
                                                       memcpy(buf + off, p + done, chunk);
                                                       bdev_write(in->i_block[blk_idx], buf);

                                                       done += chunk;
                                                   }
                                                   if (pos + len > in->i_size) {
                                                       in->i_size = pos + len;
                                                       in->i_blocks = (in->i_size + 511) / 512;
                                                   }
                                                   return 0;
                                                                    }

                                                                    int berfs_dir_next(struct berfs_mount *m, const struct berfs_inode *dir,
                                                                                       uint32_t *pos, uint32_t *ino, char *name, uint8_t *type) {
                                                                        while (*pos < dir->i_size) {
                                                                            struct berfs_dirent de;
                                                                            if (file_read_bytes(m, dir, *pos, &de, sizeof(de)) != 0) return -1;
                                                                            if (de.d_inode == 0 || de.d_reclen == 0) return -1;
                                                                            if (file_read_bytes(m, dir, *pos + sizeof(de), name, de.d_name_len) != 0) return -1;
                                                                            name[de.d_name_len] = '\0';
                                                                            *ino = de.d_inode;
                                                                            *type = de.d_type;
                                                                            *pos += de.d_reclen;
                                                                            return 1;
                                                                        }
                                                                        return 0;
                                                                                       }

                                                                                       uint32_t berfs_dir_lookup(struct berfs_mount *m, const struct berfs_inode *dir, const char *name) {
                                                                                           uint32_t pos = 0, ino;
                                                                                           char buf[BERFS_NAME_MAX + 1];
                                                                                           uint8_t type;
                                                                                           while (berfs_dir_next(m, dir, &pos, &ino, buf, &type) == 1) {
                                                                                               if (strcmp(buf, name) == 0) return ino;
                                                                                           }
                                                                                           return 0;
                                                                                       }

                                                                                       int berfs_dir_add(struct berfs_mount *m, struct berfs_inode *dir_inode,
                                                                                                         uint32_t dir_ino, const char *name, uint32_t ino, uint8_t type) {
                                                                                           size_t name_len = strlen(name);
                                                                                           if (name_len == 0 || name_len > BERFS_NAME_MAX) return -1;

                                                                                           uint16_t reclen = (uint16_t)(sizeof(struct berfs_dirent) + name_len);
                                                                                           reclen = (uint16_t)((reclen + 3u) & ~3u);

                                                                                           struct berfs_dirent de;
                                                                                           de.d_inode    = ino;
                                                                                           de.d_reclen   = reclen;
                                                                                           de.d_name_len = (uint8_t)name_len;
                                                                                           de.d_type     = type;

                                                                                           uint32_t pos = dir_inode->i_size;
                                                                                           uint8_t tmp[BLOCK_SIZE];
                                                                                           memset(tmp, 0, reclen);
                                                                                           memcpy(tmp, &de, sizeof(de));
                                                                                           memcpy(tmp + sizeof(de), name, name_len);

                                                                                           if (berfs_file_write(m, dir_inode, pos, tmp, reclen) != 0) return -1;
                                                                                           if (berfs_write_inode(m, dir_ino, dir_inode) != 0) return -1;
                                                                                           return 0;
                                                                                                         }

                                                                                                         int berfs_free_block(struct berfs_mount *m, uint32_t blk) {
                                                                                                             if (blk < m->sb.sb_data_block || blk >= m->sb.sb_total_blocks) return -1;
                                                                                                             uint8_t buf[BLOCK_SIZE];
                                                                                                             uint32_t bitmap_start = m->sb.sb_block_bitmap_block;
                                                                                                             uint32_t byte_idx = blk / 8;
                                                                                                             uint32_t bit_idx = blk % 8;

                                                                                                             berfs_read_block(m, bitmap_start + (byte_idx / BLOCK_SIZE), buf);
                                                                                                             uint32_t off = byte_idx % BLOCK_SIZE;
                                                                                                             buf[off] &= ~(1 << bit_idx);
                                                                                                             bdev_write(bitmap_start + (byte_idx / BLOCK_SIZE), buf);

                                                                                                             m->sb.sb_free_blocks++;
                                                                                                             berfs_write_sb(m);
                                                                                                             return 0;
                                                                                                         }

                                                                                                         int berfs_free_inode(struct berfs_mount *m, uint32_t ino) {
                                                                                                             if (ino < 2 || ino >= m->sb.sb_total_inodes) return -1;
                                                                                                             uint8_t buf[BLOCK_SIZE];
                                                                                                             uint32_t bitmap_start = m->sb.sb_inode_bitmap_block;
                                                                                                             uint32_t byte_idx = ino / 8;
                                                                                                             uint32_t bit_idx = ino % 8;

                                                                                                             berfs_read_block(m, bitmap_start, buf);
                                                                                                             buf[byte_idx] &= ~(1 << bit_idx);
                                                                                                             bdev_write(bitmap_start, buf);

                                                                                                             m->sb.sb_free_inodes++;
                                                                                                             berfs_write_sb(m);
                                                                                                             return 0;
                                                                                                         }

                                                                                                         int berfs_dir_remove(struct berfs_mount *m, struct berfs_inode *dir_inode,
                                                                                                                              uint32_t dir_ino, const char *name) {
                                                                                                             uint32_t pos = 0;
                                                                                                             while (pos < dir_inode->i_size) {
                                                                                                                 uint32_t start_pos = pos;
                                                                                                                 struct berfs_dirent de;
                                                                                                                 if (file_read_bytes(m, dir_inode, pos, &de, sizeof(de)) != 0) return -1;
                                                                                                                 if (de.d_inode == 0) { pos += de.d_reclen; continue; }

                                                                                                                 char entry_name[BERFS_NAME_MAX + 1];
                                                                                                                 if (file_read_bytes(m, dir_inode, pos + sizeof(de), entry_name, de.d_name_len) != 0) return -1;
                                                                                                                 entry_name[de.d_name_len] = '\0';

                                                                                                                 if (strcmp(entry_name, name) == 0) {
                                                                                                                     uint32_t zero = 0;
                                                                                                                     if (berfs_file_write(m, dir_inode, start_pos, &zero, 4) != 0) return -1;
                                                                                                                     if (berfs_write_inode(m, dir_ino, dir_inode) != 0) return -1;
                                                                                                                     return 0;
                                                                                                                 }
                                                                                                                 pos += de.d_reclen;
                                                                                                             }
                                                                                                             return -2;
                                                                                                                              }
