#include "libb.h"

long _syscall6(long n, long a, long b, long c, long d, long e) {
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8  __asm__("r8")  = e;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8)
        : "memory"
    );
    return ret;
}

long read(int fd, void *buf, size_t len) { return _syscall6(SYS_READ, fd, (long)buf, len, 0, 0); }
long write(int fd, const void *buf, size_t len) { return _syscall6(SYS_WRITE, fd, (long)buf, len, 0, 0); }

int open(const char *path, int flags, ...) {
    return (int)_syscall6(SYS_OPEN, (long)path, flags, 0, 0, 0);
}

int close(int fd) { return (int)_syscall6(SYS_CLOSE, fd, 0, 0, 0, 0); }
void _exit(int code) { _syscall6(SYS_EXIT, code, 0, 0, 0, 0); for(;;) __asm__ __volatile__("hlt"); }
int mkdir(const char *path, int mode) { return (int)_syscall6(SYS_MKDIR, (long)path, mode, 0, 0, 0); }
int unlink(const char *path) { return (int)_syscall6(SYS_UNLINK, (long)path, 0, 0, 0, 0); }

size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (int)(unsigned char)*a - (int)(unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n) { while (n && *a && *a == *b) { a++; b++; n--; } return n == 0 ? 0 : (int)(unsigned char)*a - (int)(unsigned char)*b; }
char *strcpy(char *dst, const char *src) { char *d = dst; while ((*d++ = *src++)) ; return dst; }
void *memset(void *dst, int c, size_t n) { unsigned char *d = (unsigned char *)dst; while (n--) *d++ = (unsigned char)c; return dst; }
void *memcpy(void *dst, const void *src, size_t n) { unsigned char *d = (unsigned char *)dst; const unsigned char *s = (const unsigned char *)src; while (n--) *d++ = *s++; return dst; }

void putchar_(char c) { write(1, &c, 1); }
void puts(const char *s) {
    write(1, s, strlen(s));
    write(1, "\n", 1);
}

void printf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char buf[24];
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { putchar_(*p); continue; }
        p++;
        switch (*p) {
            case 's': { const char *s = __builtin_va_arg(ap, const char *); if (!s) s = "(null)"; puts(s); break; }
            case 'd': case 'u': {
                long v = __builtin_va_arg(ap, long);
                if (*p == 'd' && v < 0) { putchar_('-'); v = -v; }
                if (v == 0) { putchar_('0'); break; }
                int n = 0; while (v) { buf[n++] = '0' + (v % 10); v /= 10; } while (n) putchar_(buf[--n]);
                break;
            }
            case 'x': {
                unsigned long v = __builtin_va_arg(ap, unsigned long);
                if (v == 0) { putchar_('0'); break; }
                int n = 0; while (v) { int d = v & 0xF; buf[n++] = d < 10 ? '0'+d : 'a'+d-10; v >>= 4; } while (n) putchar_(buf[--n]);
                break;
            }
            case 'c': { int c = __builtin_va_arg(ap, int); putchar_((char)c); break; }
            case '%': putchar_('%'); break;
            default: putchar_('%'); putchar_(*p); break;
        }
    }
    __builtin_va_end(ap);
}
