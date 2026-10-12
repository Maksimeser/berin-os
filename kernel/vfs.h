#ifndef BERIN_VFS_H
#define BERIN_VFS_H

#include <stdint.h>
#include "berfs.h"
struct berfs_mount;

/* Node types */
#define VFS_FILE  1
#define VFS_DIR   2
#define VFS_DEV   3

/* Open flags */
#define O_RDONLY  0x01
#define O_WRONLY  0x02
#define O_RDWR    0x03
#define O_CREATE  0x04
#define O_TRUNC   0x08

/* Maximum open files per call */
#define VFS_MAX_FILES 32
#define VFS_MAX_MOUNTS 4
#define VFS_PATH_MAX 256

struct vfs_mount;

struct vfs_ops {
    /* read len bytes at pos, return bytes read, or -1 */
    int      (*read)(struct vfs_mount *m, uint32_t ino, uint32_t pos, void *buf, uint32_t len);
    /* lookup name in dir ino, store inode in *out_ino, return 0 */
    int      (*lookup)(struct vfs_mount *m, uint32_t dir_ino, const char *name, uint32_t *out_ino);
    /* readdir: fill name, ino, type; return 1 if entry, 0 if end, -1 error */
    int      (*readdir)(struct vfs_mount *m, uint32_t dir_ino, uint32_t *pos,
                        char *name, uint32_t *out_ino, uint8_t *type);
    /* get inode size and type */
    int      (*stat)(struct vfs_mount *m, uint32_t ino, uint32_t *size, uint8_t *type);
};

struct vfs_mount {
    char              path[VFS_PATH_MAX];   /* "/" for root */
    uint32_t          root_ino;
    const struct vfs_ops *ops;
    void             *fs_data;              /* pointer to berfs_mount */
};

/* Init VFS with no mounts */
void vfs_init(void);

/* Register a mount. Takes ownership of fs_data pointer. */
int  vfs_mount_add(const char *path, uint32_t root_ino,
                   const struct vfs_ops *ops, void *fs_data);

/* Resolve absolute path -> (mount, ino). Returns 0 on success. */
int  vfs_resolve(const char *path, struct vfs_mount **out_mount, uint32_t *out_ino);

/* Stat by path */
int  vfs_stat(const char *path, uint32_t *size, uint8_t *type);

/* Open a file, returns fd (>= 0) or negative error */
int  vfs_open(const char *path, int flags);
/* Read up to len bytes at current pos, advances pos. Returns bytes read. */
int  vfs_read(int fd, void *buf, uint32_t len);
/* Close fd */
int  vfs_close(int fd);

/* List root of a path (for bls) */
int  vfs_listdir(const char *path, uint32_t *pos,
                 char *name, uint32_t *ino, uint8_t *type);

/* Register built-in BerFS backend (called once) */
int  vfs_mount_berfs(struct berfs_mount *bm);
struct berfs_mount;
#endif
