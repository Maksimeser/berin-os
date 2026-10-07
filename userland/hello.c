/* Berin userland - hello */

static long syscall3(long n, long a, long b, long c) {
    long ret;
    register long rax __asm__("rax") = n;
    register long rdi __asm__("rdi") = a;
    register long rsi __asm__("rsi") = b;
    register long rdx __asm__("rdx") = c;
    __asm__ __volatile__(
        "int $0x80"
        : "+r"(rax)
        : "r"(rdi), "r"(rsi), "r"(rdx)
        : "memory"
    );
    ret = rax;
    return ret;
}

static long slen(const char *s) {
    long n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void) {
    const char msg[] = "Hello from Ring 3 (real ELF)!\n";
    syscall3(1, 1, (long)msg, slen(msg));
    syscall3(60, 0, 0, 0);
    for (;;) { }
}
