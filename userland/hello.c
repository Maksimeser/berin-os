/* Berin userland - hello
 * Standalone ELF64, no libc. Uses only int 0x80.
 */

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "memory"
    );
    return ret;
}

static long sys_write(const char *s, long len) {
    return syscall3(1, 1, (long)s, len);
}

static void sys_exit(long code) {
    syscall3(60, code, 0, 0);
    for (;;) __asm__ __volatile__("hlt");
}

static long strlen_(const char *s) {
    long n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void) {
    const char msg[] = "Hello from Ring 3 (real ELF)!\n";
    sys_write(msg, strlen_(msg));
    sys_exit(0);
}
