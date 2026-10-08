#include "berfs_dir.h"
#include <string.h>

/* read the byte at position pos in a file (inode), supporting only direct blocks */
static int file_read_byte(struct berfs_fs *fs, const struct berfs_inode *in,
                          uint32_t pos, uint8_t *out) {
    if (pos >= in->i_size) return -1;
    uint32_t blk_idx = pos / BERFS_BLOCK_SIZE;
    uint32_t off     = pos % BERFS_BLOCK_SIZE;
    if (blk_idx >= BERFS_DIRECT_BLOCKS) return -1;   /* v1: only direct */
        uint32_t block = in->i_block[blk_idx];
    if (block == 0) return -1;

    uint8_t buf[BERFS_BLOCK_SIZE];
    if (berfs_read_block(fs, block, buf) != 0) return -1;
    *out = buf[off];
    return 0;
                          }

                          static int file_read_bytes(struct berfs_fs *fs, const struct berfs_inode *in,
                                                     uint32_t pos, void *dst, uint32_t len) {
                              uint8_t *p = (uint8_t *)dst;
                              for (uint32_t i = 0; i < len; i++) {
                                  if (file_read_byte(fs, in, pos + i, &p[i]) != 0) return -1;
                              }
                              return 0;
                                                     }

                                                     int berfs_dir_next(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                                                                        uint32_t *pos, uint32_t *ino, char *name_buf, uint8_t *type) {
                                                         if (*pos >= dir_inode->i_size) return 0;

                                                         struct berfs_dirent de;
                                                         if (file_read_bytes(fs, dir_inode, *pos, &de, sizeof(de)) != 0) return -1;

                                                         if (de.d_inode == 0) {
                                                             /* empty slot — skip by reclen */
                                                             *pos += de.d_reclen;
                                                             return berfs_dir_next(fs, dir_inode, pos, ino, name_buf, type);
                                                         }

                                                         if (file_read_bytes(fs, dir_inode, *pos + sizeof(de),
                                                             name_buf, de.d_name_len) != 0) return -1;
                                                         name_buf[de.d_name_len] = '\0';

                                                         *ino  = de.d_inode;
                                                         *type = de.d_type;
                                                         *pos += de.d_reclen;
                                                         return 1;
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
