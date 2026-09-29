/* Berin OS - berbash 0.1*/
#include "i18n.h"
#include "heap.h"
#include <stdint.h>
#include <stddef.h>
#include "serial.h"
#include "keyboard.h"
#include "timer.h"
#include "rtc.h"
#include "version.h"
#include "pmm.h"
#include "paging.h"
#include "wait.h"

extern void term_write(const char *s);
extern void term_write(const char *s);
extern void term_putchar(char c);
extern void term_set_fg(uint8_t fg);
extern void term_set_bg(uint8_t bg);
extern void term_reset_color(void);
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

/* ---------- low-level helpers ---------- */

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

static void put_hex32(uint32_t v) {
    for (int i = 28; i >= 0; i -= 4)
        term_putchar("0123456789ABCDEF"[(v >> i) & 0xF]);
}

/* ---------- input: keyboard or serial ---------- */

static char input_getchar(void) {
    for (;;) {
        if (kb_ready())        return kb_getchar();
        if (serial_rx_ready()) return serial_read_char();
        __asm__ __volatile__("hlt");
    }
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
    term_write("  versi [comp]  versions (v-/l-/h- flags)\n");
    term_write("  meminfo       physical memory summary\n");
    term_write("  memtest       alloc 3 pages, free, verify\n");
    term_write("  paging        paging status (CR3, CR2)\n");
    term_write("  pf            trigger a page fault (test)\n");
    term_write("  heap          kernel heap usage\n");
    term_write("  kmtest        kmalloc/kfree round-trip test\n");
    term_write("  locale        show/set language (en|ru)\n");
    term_write("  wtest         wait spinner demo (OK)\n");
    term_write("  wfail         wait spinner demo (FAIL)\n");
    term_write("  ptest         progress bar demo\n");
    term_write("  colors        ANSI color demo\n");
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
    { "pmm",      "0.1",         "bitmap, 4K pages" },
    { "paging",   "0.1",         "2-level, identity-map" },
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
        term_reset_color();
        term_set_fg(C_RED);
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

/* ---------- memory / paging ---------- */

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

static int builtin_paging(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("paging:\n");
    term_write("  enabled : ");
    term_write(paging_enabled() ? "yes" : "no");
    term_putchar('\n');
    term_write("  PDEs    : "); put_u32(paging_pde_count());
    term_write("   (covers "); put_u32(paging_pde_count() * 4);
    term_write(" MB)\n");
    term_write("  CR3     : 0x"); put_hex32(paging_get_cr3()); term_putchar('\n');
    term_write("  CR2     : 0x"); put_hex32(paging_get_cr2()); term_putchar('\n');

    volatile uint32_t *probe = (volatile uint32_t *)0x100000;
    uint32_t v = *probe;
    term_write("  probe[0x100000] = 0x"); put_hex32(v); term_putchar('\n');
    term_write("  identity map: OK\n");
    return 0;
}

static int builtin_heap(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("kernel heap:\n");
    term_write("  total: "); put_u32(heap_total()); term_write(" B\n");
    term_write("  used:  "); put_u32(heap_used());  term_write(" B\n");
    term_write("  free:  "); put_u32(heap_free());  term_write(" B\n");
    return 0;
}

static int builtin_colors(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("foreground colors:\n");
    term_write("\033[30m 0:black ");
    term_write("\033[31m 1:red ");
    term_write("\033[32m 2:green ");
    term_write("\033[33m 3:yellow ");
    term_write("\033[34m 4:blue ");
    term_write("\033[35m 5:magenta ");
    term_write("\033[36m 6:cyan ");
    term_write("\033[37m 7:white\n");
    term_write("\033[90m 8:br-black ");
    term_write("\033[91m 9:br-red ");
    term_write("\033[92m 10:br-green ");
    term_write("\033[93m 11:br-yellow ");
    term_write("\033[94m 12:br-blue ");
    term_write("\033[95m 13:br-magenta ");
    term_write("\033[96m 14:br-cyan ");
    term_write("\033[97m 15:br-white\n");
    term_write("\033[0mreset done\n");
    return 0;
}

static int builtin_pf(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("[ WARN ] pf: reading unmapped address 0xDEADBEEF\n");
    volatile uint32_t *bad = (volatile uint32_t *)0xDEADBEEF;
    uint32_t v = *bad;
    (void)v;
    term_write("if you see this, paging is broken\n");
    return 0;
}

static int builtin_kmtest(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t before = heap_used();

    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(32);

    term_write("a=0x"); put_hex32((uint32_t)a); term_putchar('\n');
    term_write("b=0x"); put_hex32((uint32_t)b); term_putchar('\n');
    term_write("c=0x"); put_hex32((uint32_t)c); term_putchar('\n');

    /* write test bytes — must not fault */
    if (a) { for (int i = 0; i < 64;  i++) ((uint8_t *)a)[i] = 0xAA; }
    if (b) { for (int i = 0; i < 128; i++) ((uint8_t *)b)[i] = 0xBB; }
    if (c) { for (int i = 0; i < 32;  i++) ((uint8_t *)c)[i] = 0xCC; }

    kfree(b);
    kfree(a);
    kfree(c);

    uint32_t after = heap_used();
    if (after == before) term_write("heap restored: OK\n");
    else                 term_write("heap MISMATCH\n");
    return 0;
}

/* ---------- dispatch table ---------- */

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static void busy_delay(uint32_t loops) {
    for (volatile uint32_t i = 0; i < loops; i++) __asm__ __volatile__("nop");
}

static int builtin_wtest(int argc, char **argv) {
    (void)argc; (void)argv;
    wait_start("searching");
    for (int i = 0; i < 20; i++) { wait_step(); busy_delay(2000000); }
    wait_done_ok();
    return 0;
}

static int builtin_wfail(int argc, char **argv) {
    (void)argc; (void)argv;
    wait_start("looking for files");
    for (int i = 0; i < 20; i++) { wait_step(); busy_delay(2000000); }
    wait_done_fail("not_found");
    return 0;
}

static int builtin_ptest(int argc, char **argv) {
    (void)argc; (void)argv;
    progress_start("downloading", 100);
    for (uint32_t i = 0; i <= 100; i += 5) {
        progress_update(i);
        busy_delay(3000000);
    }
    progress_done();
    return 0;
}

static int builtin_locale(int argc, char **argv) {
    if (argc == 1) {
        term_write(msg(MSG_LOCALE_CURRENT));
        term_putchar(' ');
        term_write(lang_name(i18n_get()));
        term_putchar('\n');
        return 0;
    }
    if (argc == 3 && str_eq(argv[1], "set")) {
        if      (str_eq(argv[2], "ru")) i18n_set(LANG_RU);
        else if (str_eq(argv[2], "en")) i18n_set(LANG_EN);
        else {
            term_set_fg(C_RED);
            term_write("invalid_argument: ");
            term_write(argv[2]);
            term_putchar('\n');
            term_set_fg(C_GREEN);
            return 0;
        }
        term_write(msg(MSG_INFO));
        term_putchar(' ');
        term_write(msg(MSG_LOCALE_SET));
        term_putchar(' ');
        term_write(lang_name(i18n_get()));
        term_putchar('\n');
        return 0;
    }
    term_write("usage: locale | locale set en|ru\n");
    return 0;
}

static struct builtin builtins[] = {
    { "echo",    builtin_echo    },
    { "help",    builtin_help    },
    { "clear",   builtin_clear   },
    { "logoff",  builtin_logoff  },
    { "crash",   builtin_crash   },
    { "uptime",  builtin_uptime  },
    { "date",    builtin_date    },
    { "time",    builtin_time    },
    { "versi",   builtin_versi   },
    { "meminfo", builtin_meminfo },
    { "memtest", builtin_memtest },
    { "paging",  builtin_paging  },
    { "pf",      builtin_pf      },
    { "heap",    builtin_heap    },
    { "kmtest",  builtin_kmtest  },
    { "colors",  builtin_colors  },
    { "locale",  builtin_locale  },
    { "wtest",   builtin_wtest   },
    { "wfail",   builtin_wfail   },
    { "ptest",   builtin_ptest   },
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

    if (i18n_get() == LANG_RU) {
        term_write("команда не найдена: ");
        term_write(name);
        term_putchar('\n');
    }
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
