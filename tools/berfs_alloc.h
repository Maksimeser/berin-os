#ifndef BERFS_ALLOC_H
#define BERFS_ALLOC_H

#include "berfs_host.h"

/* Allocate a free inode. Returns inode number (>0), or 0 on failure. */
uint32_t berfs_inode_alloc(struct berfs_fs *fs);

/* Free an inode (0-based inode number). */
int      berfs_inode_free(struct berfs_fs *fs, uint32_t ino);

/* Allocate a free data block. Returns block number, or 0 on failure. */
uint32_t berfs_block_alloc(struct berfs_fs *fs);

/* Free a data block. */
int      berfs_block_free(struct berfs_fs *fs, uint32_t block);

#endif
