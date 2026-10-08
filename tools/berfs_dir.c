#include "berfs_dir.h"
#include "berfs_alloc.h"
#include <string.h>

static int file_read_bytes(struct berfs_fs *fs, const struct berfs_inode *in,
                           uint32_t pos, void *dst, uint32_t len) {
    uint8_t *p = (uint8_t *)dst;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t bp = pos + i;
        if (bp >= in->i_size) return -1;
        uint32_t blk_idx = bp / BERFS_BLOCK_SIZE;
        uint32_t off     = bp % BERFS_BLOCK_SIZE;
        if (blk_idx >= BERFS_DIRECT_BLOCKS) return -1;
        uint32_t block = in->i_block[blk_idx];
        if (block == 0) return -1;
        uint8_t buf[BERFS_BLOCK_SIZE];
        if (berfs_read_block(fs, block, buf) != 0) return -1;
        p[i] = buf[off];
    }
    return 0;
                           }

                           int berfs_file_write(struct berfs_fs *fs, struct berfs_inode *in,
                                                uint32_t pos, const void *src, uint32_t len) {
                               const uint8_t *p = (const uint8_t *)src;
                               uint32_t done = 0;
                               while (done < len) {
                                   uint32_t bp = pos + done;
                                   uint32_t blk_idx = bp / BERFS_BLOCK_SIZE;
                                   uint32_t off     = bp % BERFS_BLOCK_SIZE;
                                   if (blk_idx >= BERFS_DIRECT_BLOCKS) return -1;

                                   uint32_t chunk = BERFS_BLOCK_SIZE - off;
                                   if (chunk > len - done) chunk = len - done;

                                   if (in->i_block[blk_idx] == 0) {
                                       uint32_t new_blk = berfs_block_alloc(fs);
                                       if (new_blk == 0) return -1;
                                       in->i_block[blk_idx] = new_blk;
                                       uint8_t zero[BERFS_BLOCK_SIZE];
                                       memset(zero, 0, sizeof(zero));
                                       if (berfs_write_block(fs, new_blk, zero) != 0) return -1;
                                   }

                                   uint8_t buf[BERFS_BLOCK_SIZE];
                                   if (berfs_read_block(fs, in->i_block[blk_idx], buf) != 0) return -1;
                                   memcpy(buf + off, p + done, chunk);
                                   if (berfs_write_block(fs, in->i_block[blk_idx], buf) != 0) return -1;

                                   done += chunk;
                               }
                               if (pos + len > in->i_size) {
                                   in->i_size = pos + len;
                                   in->i_blocks = (in->i_size + 511u) / 512u;
                               }
                               return 0;
                                                }

                                                int berfs_dir_next(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                                                                   uint32_t *pos, uint32_t *ino, char *name_buf, uint8_t *type) {
                                                    while (*pos < dir_inode->i_size) {
                                                        struct berfs_dirent de;
                                                        if (file_read_bytes(fs, dir_inode, *pos, &de, sizeof(de)) != 0) return -1;

                                                        if (de.d_inode == 0 || de.d_reclen == 0) {
                                                            /* corrupted or end — bail */
                                                            return -1;
                                                        }

                                                        if (file_read_bytes(fs, dir_inode, *pos + sizeof(de),
                                                            name_buf, de.d_name_len) != 0) return -1;
                                                        name_buf[de.d_name_len] = '\0';

                                                        *ino  = de.d_inode;
                                                        *type = de.d_type;
                                                        *pos += de.d_reclen;
                                                        return 1;
                                                    }
                                                    return 0;
                                                                   }

                                                                   uint32_t berfs_dir_lookup(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                                                                                             const char *name) {
                                                                       uint32_t pos = 0;
                                                                       uint32_t ino;
                                                                       char buf[BERFS_NAME_MAX + 1];
                                                                       uint8_t type;
                                                                       int r;
                                                                       while ((r = berfs_dir_next(fs, dir_inode, &pos, &ino, buf, &type)) == 1) {
                                                                           if (strcmp(buf, name) == 0) return ino;
                                                                       }
                                                                       return 0;
                                                                                             }

                                                                                             int berfs_dir_add(struct berfs_fs *fs, struct berfs_inode *dir_inode,
                                                                                                               uint32_t dir_ino, const char *name, uint32_t ino, uint8_t type) {
                                                                                                 size_t name_len = strlen(name);
                                                                                                 if (name_len == 0 || name_len > BERFS_NAME_MAX) return -1;

                                                                                                 uint16_t reclen = (uint16_t)(sizeof(struct berfs_dirent) + name_len);
                                                                                                 /* pad to multiple of 4 */
                                                                                                 reclen = (uint16_t)((reclen + 3u) & ~3u);

                                                                                                 struct berfs_dirent de;
                                                                                                 de.d_inode    = ino;
                                                                                                 de.d_reclen   = reclen;
                                                                                                 de.d_name_len = (uint8_t)name_len;
                                                                                                 de.d_type     = type;

                                                                                                 uint32_t pos = dir_inode->i_size;

                                                                                                 /* We might need to fit name + padding. Use a temp buffer. */
                                                                                                 uint8_t tmp[BERFS_BLOCK_SIZE];
                                                                                                 memset(tmp, 0, reclen);
                                                                                                 memcpy(tmp, &de, sizeof(de));
                                                                                                 memcpy(tmp + sizeof(de), name, name_len);

                                                                                                 if (berfs_file_write(fs, dir_inode, pos, tmp, reclen) != 0) return -1;

                                                                                                 /* persist directory inode */
                                                                                                 if (berfs_write_inode(fs, dir_ino, dir_inode) != 0) return -1;
                                                                                                 return 0;
                                                                                                               }
