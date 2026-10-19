/* Berin - syscall dispatcher (int 0x80)
 * ABI: rax=number, rdi/rsi/rdx/r10 = args, return in rax
 */

#include "syscall.h"
#include "vfs.h"
#include "keyboard.h"

extern char berin_input_getchar(void);
extern void term_write(const char *s);
extern void term_putchar(char c);

static void put_u64(uint64_t v) {
    char r[24]; int ri = 0;
    if (v == 0) { term_putchar('0'); return; }
    while (v) { r[ri++] = '0' + (v % 10); v /= 10; }
    while (ri) term_putchar(r[--ri]);
}

/* sys_write(fd, buf, len) */
static uint64_t sys_write(uint64_t fd, const void *buf, uint64_t len) {
    if (fd == 1 || fd == 2) {
        const char *s = (const char *)buf;
        for (uint64_t i = 0; i < len; i++) {
            term_putchar(s[i]);
        }
        return len;
    }
    int r = vfs_write((int)fd, buf, (uint32_t)len);
    if (r < 0) return (uint64_t)-1;
    return (uint64_t)r;
}

/* sys_open(path, flags) */
static uint64_t sys_open(const char *path, uint64_t flags) {
    if (!path) return (uint64_t)-1;
    if ((flags & 0x04) && vfs_resolve(path, 0, 0) != 0) {
        vfs_create(path);
    }
    int fd = vfs_open(path, (int)flags);
    if (fd < 0) return (uint64_t)-1;
    return (uint64_t)fd;
}

static uint64_t sys_read(uint64_t fd, void *buf, uint64_t len) {
    if (fd == 0) {
        __asm__ __volatile__("sti");
        uint8_t *p = (uint8_t *)buf;
        uint64_t n = 0;
        while (n < len) {
            char c = berin_input_getchar();
            if (c == 3) {
                return (uint64_t)-2;
            }
            if (c == '\r' || c == '\n') {
                term_putchar('\n');
                p[n++] = '\n';
                break;
            }
            if (c == 0x7F || c == '\b') {
                if (n > 0) {
                    n--;
                    term_putchar('\b');
                    term_putchar(' ');
                    term_putchar('\b');
                }
                continue;
            }
            if ((unsigned char)c < 0x20) continue;
            p[n++] = (uint8_t)c;
            term_putchar(c);
        }
        return n;
    }
    int n = vfs_read((int)fd, buf, (uint32_t)len);
    if (n < 0) return (uint64_t)-1;
    return (uint64_t)n;
}

/* sys_close(fd) */
static uint64_t sys_close(uint64_t fd) {
    return (uint64_t)vfs_close((int)fd);
}

/* sys_stat(path, buf*) where buf is: struct { u32 size; u8 type; ... } */
struct sys_stat_buf {
    uint32_t size;
    uint8_t  type;
    uint8_t  pad[3];
};
static uint64_t sys_stat(const char *path, struct sys_stat_buf *out) {
    if (!path || !out) return (uint64_t)-1;
    uint32_t size; uint8_t type;
    if (vfs_stat(path, &size, &type) != 0) return (uint64_t)-1;
    out->size = size;
    out->type = type;
    return 0;
}

/* sys_listdir(path, pos*, name*, ino*, type*) */
static uint64_t sys_listdir(const char *path, uint32_t *pos,
                            char *name, uint32_t *ino, uint8_t *type) {
    if (!path || !pos || !name || !ino || !type) return (uint64_t)-1;
    int r = vfs_listdir(path, pos, name, ino, type);
    if (r < 0) return (uint64_t)-1;
    return (uint64_t)r;   /* 1 = entry, 0 = end */
                            }

                            /* sys_exit_to_kernel(code) */
                            extern void exit_to_kernel(uint64_t code);
                            extern void reboot_now(void);
                            extern void logoff_now(void);

                            static uint64_t sys_exit(uint64_t code) {
                                term_write("\n[user] exit(");
                                put_u64(code);
                                term_write(")\n");

                                if (code == 100) reboot_now();
                                if (code == 101) logoff_now();

                                exit_to_kernel(code);
                                return 0;
                            }
                            extern int vfs_unlink(const char *path);

                            static uint64_t sys_unlink(const char *path) {
                                if (!path) return (uint64_t)-1;
                                return (uint64_t)vfs_unlink(path);
                            }

                            static uint64_t sys_mkdir(const char *path, uint64_t mode) {
                                (void)mode;
                                if (!path) return (uint64_t)-1;
                                return (uint64_t)vfs_mkdir(path);
                            }

                            void syscall_dispatch(struct regs *r) {
                                uint64_t nr  = r->rax;
                                uint64_t a1  = r->rdi;
                                uint64_t a2  = r->rsi;
                                uint64_t a3  = r->rdx;
                                uint64_t ret = (uint64_t)-1;

                                switch (nr) {
                                    case 83:          ret = sys_mkdir((const char *)a1, a2);      break;
                                    case 87:          ret = sys_unlink((const char *)a1);        break;
                                    case SYS_WRITE:   ret = sys_write(a1, (const char *)a2, a3); break;
                                    case SYS_OPEN:    ret = sys_open((const char *)a1, a2);      break;
                                    case SYS_READ:    ret = sys_read(a1, (void *)a2, a3);        break;
                                    case SYS_CLOSE:   ret = sys_close(a1);                       break;
                                    case SYS_STAT:    ret = sys_stat((const char *)a1, (struct sys_stat_buf *)a2); break;
                                    case SYS_LISTDIR: ret = sys_listdir((const char *)a1, (uint32_t *)a2,
                                        (char *)a3, (uint32_t *)r->r10,
                                                                        (uint8_t *)r->r8);       break;
                                    case SYS_EXIT:    sys_exit(a1);                              break;
                                    default:          term_write("[sys] unknown syscall\n");     break;
                                }
                                r->rax = ret;
                            }
