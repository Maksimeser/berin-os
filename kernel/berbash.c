/* Berin OS - berbash 0.1
 * echo, help, clear, logoff, crash, uptime.
 * Input: serial (COM1). Output: VGA + serial.
 */
#include "paging.h"
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>
#include "serial.h"
#include "keyboard.h"

static char input_getchar(void) {
    for (;;) {
        if (kb_ready())        return kb_getchar();
        if (serial_rx_ready()) return serial_read_char();
        __asm__ __volatile__("hlt");
    }
}

#include "timer.h"
#include "rtc.h"
#include "version.h"

extern void term_write(const char *s);
extern void term_putchar(char c);
extern void term_set_fg(uint8_t fg);

#define MAX_LINE 256
#define MAX_ARGS 16

#define C_GREEN   10
#define C_YELLOW  14
#define C_GRAY     7
#define C_RED     12

static char line_buf[MAX_LINE];
static int  line_len;

/* ---------- helpers ---------- */

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

static void put_u32(uint32_t v) {
    char buf[12];
    int i = 0;
    if (v == 0) { term_putchar('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i > 0) term_putchar(buf[--i]);
}

/* ---------- builtins ---------- */

static int builtin_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) term_putchar(' ');
        term_write(argv[i]);
    }
    term_putchar('\n');
    return 0;
}

static int builtin_help(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("berbash 0.1 - builtins:\n");
    term_write("  echo <args>   print arguments\n");
    term_write("  help          this message\n");
    term_write("  clear         clear screen\n");
    term_write("  logoff        shut down / halt\n");
    term_write("  crash         trigger a kernel panic (test)\n");
    term_write("  uptime        show time since boot\n");
    term_write("  date          show current date\n");
    term_write("  time          show current time\n");
    term_write("  versi [comp]  show versions (v-/l-/h- flags)\n");
    term_write("  meminfo       physical memory summary\n");
    term_write("  memtest       alloc 3 pages, free them, verify\n");
    term_write("  paging        paging status (CR3, CR2, sanity check)\n");
    return 0;
}

static int builtin_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < 25; i++) term_putchar('\n');
    return 0;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" :: "a"(val), "Nd"(port));
}

static int builtin_logoff(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("\n[ INFO ] logoff: halting\n");
    outw(0x604,  0x2000);
    outw(0x4004, 0x2000);
    return 42;
}

static int builtin_crash(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("[ WARN ] crash: triggering divide error\n");
    __asm__ __volatile__(
        "movl $0, %%eax\n\t"
        "movl $1, %%ebx\n\t"
        "divl %%eax\n\t"
        : : : "eax", "ebx", "edx");
    return 0;
}
static int builtin_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t t  = (uint32_t)timer_ticks();
    uint32_t hz = timer_hz();
    uint32_t sec  = t / hz;
    uint32_t frac = t % hz;
    term_write("up ");
    put_u32(sec);
    term_putchar('.');
    if (frac < 10) term_putchar('0');
    put_u32(frac);
    term_write(" s  (");
    put_u32(t);
    term_write(" ticks @ ");
    put_u32(hz);
    term_write(" Hz)\n");
    return 0;
}

/* ---------- versi ---------- */

struct version_entry {
    const char *name;
    const char *version;
    const char *note;
};

static const struct version_entry versions[] = {
    { "kernel",   BERIN_VERSION, "i386, protected mode" },
    { "berbash",  "0.1",         "shell" },
    { "serial",   "0.1",         "COM1 115200 8N1" },
    { "keyboard", "0.1",         "PS/2 scancode set 1" },
    { "rtc",      "0.1",         "CMOS 0x70/0x71" },
    { "panic",    "0.1",         "(1panic1)-NNN_name" },
    { "gdt",      "0.1",         "flat model" },
    { "idt",      "0.1",         "256 vectors" },
    { "pic",      "0.1",         "remap 32/40" },
    { "pit",      "0.1",         "100 Hz" },
    { "timer",    "0.1",         "tick counter" },
    { NULL, NULL, NULL }
};

static int is_flag_token(const char *s) {
    const char *p = s;
    if (!*p) return 0;
    while (*p) p++;
    return *(p - 1) == '-';
}

static int flag_has(const char *flags, char c) {
    const char *p = flags;
    while (*p && *p != '-') {
        if (*p == c) return 1;
        p++;
    }
    return 0;
}

static int builtin_versi(int argc, char **argv) {
    int verbose = 0;
    int list_only = 0;
    const char *target = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (is_flag_token(a)) {
            if (flag_has(a, 'v')) verbose = 1;
            if (flag_has(a, 'l')) list_only = 1;
            if (flag_has(a, 'h')) {
                term_write("versi - versioner\n");
                term_write("usage: versi [component] [flags-]\n");
                term_write("flags (suffix dash):\n");
                term_write("  v-   verbose (show notes)\n");
                term_write("  l-   list component names only\n");
                term_write("  h-   this help\n");
                return 0;
            }
        } else {
            target = a;
        }
    }

    if (list_only) {
        for (int i = 0; versions[i].name; i++) {
            if (i > 0) term_putchar(' ');
            term_write(versions[i].name);
        }
        term_putchar('\n');
        return 0;
    }

    if (target) {
        for (int i = 0; versions[i].name; i++) {
            if (str_eq(versions[i].name, target)) {
                term_write(versions[i].name);
                term_write("  ");
                term_write(versions[i].version);
                term_putchar('\n');
                if (verbose && versions[i].note) {
                    term_write("  note: ");
                    term_write(versions[i].note);
                    term_putchar('\n');
                }
                return 0;
            }
        }
        term_set_fg(C_RED);
        term_write("version_not_found: ");
        term_write(target);
        term_putchar('\n');
        term_set_fg(C_GREEN);
        return 0;
    }

    term_write("Berin OS ");
    term_write(BERIN_VERSION);
    term_write(" \"" BERIN_CODENAME "\"\n");
    for (int i = 0; versions[i].name; i++) {
        term_write("  ");
        term_write(versions[i].name);
        int len = 0;
        const char *p = versions[i].name;
        while (*p++) len++;
        for (int k = len; k < 10; k++) term_putchar(' ');
        term_write(versions[i].version);
        if (verbose && versions[i].note) {
            term_write("   ");
            term_write(versions[i].note);
        }
        term_putchar('\n');
    }
    term_write("\nvendor: ");
    term_write(BERIN_VENDOR);
    term_write("  arch: ");
    term_write(BERIN_ARCH);
    term_putchar('\n');
    return 0;
}

/* ---------- dispatch table ---------- */

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static void put_hex32(uint32_t v) {
    for (int i = 28; i >= 0; i -= 4)
        term_putchar("0123456789ABCDEF"[(v >> i) & 0xF]);
}

static int builtin_meminfo(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t total = pmm_total_pages();
    uint32_t free_ = pmm_free_pages();
    uint32_t used  = pmm_used_pages();

    term_write("physical memory (4K pages):\n");
    term_write("  total: "); put_u32(total); term_write(" pages = "); put_u32(total * 4 / 1024); term_write(" MB\n");
    term_write("  used:  "); put_u32(used);  term_write(" pages = "); put_u32(used  * 4 / 1024); term_write(" MB\n");
    term_write("  free:  "); put_u32(free_); term_write(" pages = "); put_u32(free_ * 4 / 1024); term_write(" MB\n");
    return 0;
}

static int builtin_paging(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("paging:\n");
    term_write("  enabled : ");
    term_write(paging_enabled() ? "yes" : "no");
    term_putchar('\n');
    term_write("  CR3     : 0x"); put_hex32(paging_get_cr3()); term_putchar('\n');
    term_write("  CR2     : 0x"); put_hex32(paging_get_cr2()); term_putchar('\n');

    /* Identity-map sanity: reading address 0x100000 (kernel load) must work. */
    volatile uint32_t *probe = (volatile uint32_t *)0x100000;
    uint32_t v = *probe;
    term_write("  probe[0x100000] = 0x"); put_hex32(v); term_putchar('\n');
    term_write("  identity map works: ");
    term_write("OK (no #PF)\n");
    return 0;
}

static int builtin_memtest(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t before = pmm_free_pages();

    void *a = pmm_alloc_page();
    void *b = pmm_alloc_page();
    void *c = pmm_alloc_page();

    term_write("alloc a: 0x"); put_hex32((uint32_t)a); term_putchar('\n');
    term_write("alloc b: 0x"); put_hex32((uint32_t)b); term_putchar('\n');
    term_write("alloc c: 0x"); put_hex32((uint32_t)c); term_putchar('\n');

    pmm_free_page(b);
    pmm_free_page(a);
    pmm_free_page(c);

    uint32_t after = pmm_free_pages();
    if (after == before) term_write("free pages restored: OK\n");
    else                 term_write("free pages MISMATCH\n");
    return 0;
}

static void put_u8_2(uint8_t v) {
    term_putchar('0' + (v / 10) % 10);
    term_putchar('0' +  v       % 10);
}

static void put_u16_4(uint16_t v) {
    term_putchar('0' + (v / 1000) % 10);
    term_putchar('0' + (v /  100) % 10);
    term_putchar('0' + (v /   10) % 10);
    term_putchar('0' +  v         % 10);
}

static int builtin_date(int argc, char **argv) {
    (void)argc; (void)argv;
    struct rtc_time t;
    rtc_read(&t);
    put_u16_4(t.year); term_putchar('-');
    put_u8_2(t.month); term_putchar('-');
    put_u8_2(t.day);
    term_putchar('\n');
    return 0;
}

static int builtin_time(int argc, char **argv) {
    (void)argc; (void)argv;
    struct rtc_time t;
    rtc_read(&t);
    put_u8_2(t.hour); term_putchar(':');
    put_u8_2(t.min);  term_putchar(':');
    put_u8_2(t.sec);
    term_putchar('\n');
    return 0;
}

static struct builtin builtins[] = {
    { "echo",   builtin_echo   },
    { "help",   builtin_help   },
    { "clear",  builtin_clear  },
    { "logoff", builtin_logoff },
    { "crash",  builtin_crash  },
    { "uptime", builtin_uptime },
    { "date",   builtin_date   },
    { "time",   builtin_time   },
    { "versi",  builtin_versi  },
    { "meminfo", builtin_meminfo },
    { "memtest", builtin_memtest },
    { "paging", builtin_paging },
    { NULL, NULL },
};

/* ---------- prompt / errors ---------- */

static void print_prompt(void) {
    term_set_fg(C_GREEN);  term_write("berin");
    term_set_fg(C_YELLOW); term_write(":root");
    term_set_fg(C_GRAY);   term_write(":~# ");
    term_set_fg(C_GREEN);
}

static void print_command_not_found(const char *name) {
    term_set_fg(C_RED);
    term_write("command_not_found: ");
    term_write(name);
    term_putchar('\n');
    term_set_fg(C_GREEN);
}

/* ---------- line editor ---------- */

static void line_edit(void) {
    line_len = 0;
    for (;;) {
        char c = input_getchar();

        if (c == '\r' || c == '\n') {
            term_putchar('\n');
            line_buf[line_len] = '\0';
            return;
        }

        if (c == 0x7F || c == '\b') {
            if (line_len > 0) {
                line_len--;
                term_putchar('\b');
                term_putchar(' ');
                term_putchar('\b');
            }
            continue;
        }

        if ((unsigned char)c < 0x20) continue;

        if (line_len < MAX_LINE - 1) {
            line_buf[line_len++] = c;
            term_putchar(c);
        }
    }
}

static int tokenize(char *s, char **argv, int max) {
    int argc = 0;
    while (*s && argc < max) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        argv[argc++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
        if (*s) *s++ = '\0';
    }
    return argc;
}

/* ---------- entry ---------- */

void berbash_run(void) {
    term_write("berbash 0.1\n");
    term_write("type 'help' for builtins, 'logoff' to shut down\n\n");

    for (;;) {
        print_prompt();
        line_edit();

        if (line_len == 0) continue;

        char *argv[MAX_ARGS];
        int argc = tokenize(line_buf, argv, MAX_ARGS);
        if (argc == 0) continue;

        const char *cmd = argv[0];
        int found = 0;
        for (int i = 0; builtins[i].name; i++) {
            if (str_eq(builtins[i].name, cmd)) {
                int rc = builtins[i].fn(argc, argv);
                if (rc == 42) return;
                found = 1;
                break;
            }
        }
        if (!found) print_command_not_found(cmd);
    }
}
