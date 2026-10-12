/* Berin - syscall dispatcher (int 0x80)
 * ABI: rax=number, rdi/rsi/rdx/r10 = args, return in rax
 */

#include "syscall.h"
#include "vfs.h"

extern void term_write(const char *s);
extern void term_putchar(char c);

static void put_u64(uint64_t v) {
    char r[24]; int ri = 0;
    if (v == 0) { term_putchar('0'); return; }
    while (v) { r[ri++] = '0' + (v % 10); v /= 10; }
    while (ri) term_putchar(r[--ri]);
}

/* sys_write(fd, buf, len) */
static uint64_t sys_write(uint64_t fd, const char *buf, uint64_t len) {
    if (fd != 1 && fd != 2) return (uint64_t)-1;
    for (uint64_t i = 0; i < len; i++) term_putchar(buf[i]);
    return len;
}

/* sys_open(path, flags) */
static uint64_t sys_open(const char *path, uint64_t flags) {
    if (!path) return (uint64_t)-1;
    int fd = vfs_open(path, (int)flags);
    if (fd < 0) return (uint64_t)-1;
    return (uint64_t)fd;
}

/* sys_read(fd, buf, len) */
static uint64_t sys_read(uint64_t fd, void *buf, uint64_t len) {
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

                            /* sys_exit(code) */
                            static uint64_t sys_exit(uint64_t code) {
                                term_write("\n[user] exit(");
                                put_u64(code);
                                term_write(") — process ended\n");
                                /* for now: halt */
                                for (;;) __asm__ __volatile__("cli; hlt");
                                return 0;
                            }

                            void syscall_dispatch(struct regs *r) {
                                uint64_t nr  = r->rax;
                                uint64_t a1  = r->rdi;
                                uint64_t a2  = r->rsi;
                                uint64_t a3  = r->rdx;
                                uint64_t ret = (uint64_t)-1;

                                switch (nr) {
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
