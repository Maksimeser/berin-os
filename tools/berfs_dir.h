#ifndef BERFS_DIR_H
#define BERFS_DIR_H

#include <stdint.h>
#include "berfs_host.h"

/* Read one directory entry starting at *pos in the directory inode.
 * Returns 1 (found), 0 (end), -1 (error). Advances *pos. */
int berfs_dir_next(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                   uint32_t *pos, uint32_t *ino, char *name_buf, uint8_t *type);

/* Find a name in a directory. Returns inode number or 0 if not found. */
uint32_t berfs_dir_lookup(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                          const char *name);

/* Add a new entry to the directory. Allocates a data block if needed.
 * Updates *dir_inode in place (i_size, i_block[]).
 * Writes dir_inode back to disk on success. Returns 0 on success. */
int berfs_dir_add(struct berfs_fs *fs, struct berfs_inode *dir_inode,
                  uint32_t dir_ino, const char *name, uint32_t ino, uint8_t type);

/* Write bytes at position in a file inode. Allocates blocks as needed.
 * Updates inode fields i_size and i_blocks in memory. Caller must
 * berfs_write_inode afterwards to persist it. */
int berfs_file_write(struct berfs_fs *fs, struct berfs_inode *in,
                     uint32_t pos, const void *src, uint32_t len);

#endif
