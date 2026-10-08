#ifndef BERFS_DIR_H
#define BERFS_DIR_H

#include <stdint.h>
#include "berfs_host.h"

/* Directory entry iterator — walks a directory file block by block. */

/* Read one directory entry starting at *pos within the directory inode.
 * On success, fills ino (inode number), name (NUL-terminated), type.
 * Advances *pos past the entry.
 * Returns 1 on entry found, 0 on end of directory, -1 on error.
 * name_buf must be at least BERFS_NAME_MAX+1 bytes. */
int berfs_dir_next(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                   uint32_t *pos, uint32_t *ino, char *name_buf, uint8_t *type);

/* Find a name in a directory. Returns inode number or 0 if not found. */
uint32_t berfs_dir_lookup(struct berfs_fs *fs, const struct berfs_inode *dir_inode,
                          const char *name);

#endif
