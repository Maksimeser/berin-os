/* Berin - syscall dispatcher (int 0x80)
 * ABI: rax=number, rdi/rsi/rdx = args, return in rax
 */

#include "syscall.h"

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

/* sys_exit(code) — halts */
static uint64_t sys_exit(uint64_t code) {
    term_write("\n[user] exit(");
    put_u64(code);
    term_write(") — system halted\n");
    for (;;) __asm__ __volatile__("cli; hlt");
    return 0;   /* unreachable, silences warning */
}

void syscall_dispatch(struct regs *r) {
    uint64_t nr = r->rax;
    uint64_t ret = (uint64_t)-1;

    switch (nr) {
        case 1:  ret = sys_write(r->rdi, (const char *)r->rsi, r->rdx); break;
        case 60: sys_exit(r->rdi);                                      break;
        default: term_write("[sys] unknown syscall\n");                 break;
    }
    r->rax = ret;
}
