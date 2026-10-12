/* Berin OS userland - hello v2 (with VFS test) */

static long syscall6(long n, long a, long b, long c, long d, long e) {
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

static long sys_write(int fd, const char *s, long len) {
    return syscall6(1, fd, (long)s, len, 0, 0);
}
static long sys_open(const char *path, int flags) {
    return syscall6(2, (long)path, flags, 0, 0, 0);
}
static long sys_read(int fd, void *buf, long len) {
    return syscall6(0, fd, (long)buf, len, 0, 0);
}
static long sys_close(int fd) {
    return syscall6(3, fd, 0, 0, 0, 0);
}
static void sys_exit(long code) {
    syscall6(60, code, 0, 0, 0, 0);
    for (;;) __asm__ __volatile__("hlt");
}

static long strlen_(const char *s) {
    long n = 0; while (s[n]) n++; return n;
}

void _start(void) {
    const char banner[] = "[user] hello from ring 3\n";
    sys_write(1, banner, strlen_(banner));

    /* open /hello.txt */
    long fd = sys_open("/hello.txt", 0);
    if (fd < 0) {
        const char err[] = "[user] not_found: /hello.txt\n";
        sys_write(1, err, strlen_(err));
        sys_exit(1);
    }
    const char opened[] = "[user] opened /hello.txt, content:\n";
    sys_write(1, opened, strlen_(opened));

    char buf[64];
    long n;
    while ((n = sys_read(fd, buf, sizeof(buf))) > 0) {
        sys_write(1, buf, n);
    }
    sys_close(fd);

    const char bye[] = "[user] done\n";
    sys_write(1, bye, strlen_(bye));
    sys_exit(0);
}
