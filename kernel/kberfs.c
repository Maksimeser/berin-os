#include "kberfs.h"
#include "blockdev.h"
#include "kstring.h"

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
