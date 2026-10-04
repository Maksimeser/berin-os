/* Berin - kernel x86_64 (Stage 2)
 * VGA + serial + ANSI + IDT + PIC + PIT + PS/2 keyboard + kerbash + panic
 */

#include <stdint.h>
#include <stddef.h>
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"

/* ============================================================
 *  Port I/O
 * ============================================================ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ __volatile__("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" :: "a"(val), "Nd"(port));
}

/* ============================================================
 *  Serial COM1
 * ============================================================ */
#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static int  serial_tx_empty(void) { return inb(COM1 + 5) & 0x20; }
static int  serial_rx_ready(void) { return inb(COM1 + 5) & 0x01; }

static void serial_write_char(char c) {
    while (!serial_tx_empty()) ;
    outb(COM1, (uint8_t)c);
}
static char serial_read_char(void) {
    while (!serial_rx_ready()) ;
    return (char)inb(COM1);
}

/* ============================================================
 *  VGA text mode 80x25
 * ============================================================ */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

#define C_BLACK   0
#define C_GRAY    7
#define C_LGREEN 10
#define C_WHITE  15

static size_t  term_row;
static size_t  term_col;
static uint8_t term_fg = C_GRAY;
static uint8_t term_bg = C_BLACK;
static uint8_t term_color = 0x07;

static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}
static inline void term_update_color(void) {
    term_color = (uint8_t)((term_bg << 4) | (term_fg & 0x0F));
}
static void term_set_fg(uint8_t fg) { term_fg = fg & 0x0F; term_update_color(); }
static void term_set_bg(uint8_t bg) { term_bg = bg & 0x0F; term_update_color(); }
static void term_reset_color(void)  { term_fg = C_GRAY; term_bg = C_BLACK; term_update_color(); }

void term_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = 0;
    term_col = 0;
}

static void term_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = VGA_HEIGHT - 1;
}

static void term_emit(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\b') {
        if (term_col > 0) term_col--;
    } else if (c == '\r') {
        term_col = 0;
    } else {
        if (term_row < VGA_HEIGHT && term_col < VGA_WIDTH)
            VGA_MEMORY[term_row * VGA_WIDTH + term_col] =
            vga_entry((unsigned char)c, term_color);
        term_col++;
    }
    if (term_col >= VGA_WIDTH) { term_col = 0; term_row++; }
    if (term_row >= VGA_HEIGHT) term_scroll();

    if (c == '\n') serial_write_char('\r');
    serial_write_char(c);
}

/* -------- ANSI escape parser -------- */
static int ansi_state = 0;
static int ansi_param = 0;

static void ansi_sgr(int p) {
    if (p == 0)                    term_reset_color();
    else if (p == 1)               ;
    else if (p >= 30 && p <= 37)   term_set_fg((uint8_t)(p - 30));
    else if (p >= 40 && p <= 47)   term_set_bg((uint8_t)(p - 40));
    else if (p >= 90 && p <= 97)   term_set_fg((uint8_t)(p - 90 + 8));
    else if (p >= 100 && p <= 107) term_set_bg((uint8_t)(p - 100 + 8));
}

void term_putchar(char c) {
    if (ansi_state == 2) {
        if (c >= '0' && c <= '9') { ansi_param = ansi_param * 10 + (c - '0'); return; }
        if (c == ';')             { ansi_param = 0; return; }
        ansi_state = 0;
        if (c == 'm')                          ansi_sgr(ansi_param);
        else if (c == 'J' && ansi_param == 2)  term_clear();
        else if (c == 'H')                     { term_row = 0; term_col = 0; }
        return;
    }
    if (ansi_state == 1) {
        if (c == '[') { ansi_state = 2; ansi_param = 0; return; }
        ansi_state = 0;
        return;
    }
    if (c == '\033') { ansi_state = 1; return; }
    term_emit(c);
}

void term_write(const char *s) {
    while (*s) term_putchar(*s++);
}

/* ============================================================
 *  Timer tick counter (IRQ0)
 * ============================================================ */
static volatile uint64_t ticks = 0;
void timer_tick(void) { ticks++; }

/* ============================================================
 *  Input: keyboard (QEMU window) + serial (host terminal)
 * ============================================================ */
static char input_getchar(void) {
    for (;;) {
        if (kb_ready())        return kb_getchar();
        if (serial_rx_ready()) return serial_read_char();
        __asm__ __volatile__("pause");
    }
}

/* ============================================================
 *  kerbash
 * ============================================================ */
#define MAX_LINE 256
#define MAX_ARGS 16

static char line_buf[MAX_LINE];
static int  line_len;

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == '\0' && *b == '\0';
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

static void put_u64(uint64_t v) {
    char r[24]; int ri = 0;
    if (v == 0) { term_putchar('0'); return; }
    while (v) { r[ri++] = '0' + (v % 10); v /= 10; }
    while (ri) term_putchar(r[--ri]);
}

static void reboot_now(void) {
    while (inb(0x64) & 0x02) ;
    outb(0x64, 0xFE);
    outw(0x604, 0x2000);
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ __volatile__("lidt %0" :: "m"(null_idt));
    __asm__ __volatile__("int $0x03");
    for (;;) __asm__ __volatile__("hlt");
}

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
    term_write("kerbash 0.2 (x86_64) - builtins:\n");
    term_write("  echo <args>   print arguments\n");
    term_write("  help          this message\n");
    term_write("  clear         clear screen\n");
    term_write("  about         about Berin OS\n");
    term_write("  versi         show versions\n");
    term_write("  colors        ANSI color demo\n");
    term_write("  uptime        ticks since boot\n");
    term_write("  crash         trigger divide error\n");
    term_write("  pf            trigger page fault\n");
    term_write("  reboot        reboot machine\n");
    term_write("  halt          halt CPU\n");
    return 0;
}

static int builtin_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    term_clear();
    return 0;
}

static int builtin_about(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("Berin OS v0.0.2 (x86_64)\n");
    term_write("Berin. Your own system.\n");
    term_write("vendor: Turker Games\n");
    term_write("mascot: Sorsha, green fox\n");
    return 0;
}

static int builtin_versi(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("Berin OS 0.0.2 \"Sorsha\" (x86_64)\n");
    term_write("  kernel   0.0.2   x86_64, long mode\n");
    term_write("  kerbash  0.2     kernel shell\n");
    term_write("  serial   0.1     COM1 115200 8N1\n");
    term_write("  keyboard 0.1     PS/2 scancode set 1\n");
    term_write("  idt/pic/pit 0.2  256 vectors, 100 Hz\n");
    term_write("  vga      0.2     text mode 3, 80x25\n");
    term_write("vendor: Turker Games\n");
    return 0;
}

static int builtin_colors(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("fg: ");
    for (int c = 30; c <= 37; c++) {
        char b[8]; int n = 0;
        b[n++] = 0x1B; b[n++] = '['; b[n++] = '0' + (c/10); b[n++] = '0' + (c%10); b[n++] = 'm';
        b[n++] = '0' + (c - 30); b[n++] = ' '; b[n++] = 0;
        term_write(b);
    }
    term_write("\033[0m\nbright: ");
    for (int c = 90; c <= 97; c++) {
        char b[8]; int n = 0;
        b[n++] = 0x1B; b[n++] = '['; b[n++] = '0' + (c/10); b[n++] = '0' + (c%10); b[n++] = 'm';
        b[n++] = '0' + (c - 90); b[n++] = ' '; b[n++] = 0;
        term_write(b);
    }
    term_write("\033[0m\n");
    return 0;
}

static int builtin_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t t = ticks;
    term_write("up ");
    put_u64(t / 100);
    term_putchar('.');
    term_putchar('0' + (t / 10) % 10);
    term_putchar('0' + t % 10);
    term_write(" s (");
    put_u64(t);
    term_write(" ticks @ 100 Hz)\n");
    return 0;
}

static int builtin_crash(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("[ WARN ] crash: divide error\n");
    __asm__ __volatile__(
        "movq $0, %%rax\n\t"
        "movq $1, %%rbx\n\t"
        "divq %%rax\n\t"
        : : : "rax", "rbx", "rdx");
    return 0;
}

static int builtin_pf(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("[ WARN ] pf: reading unmapped 0xDEADBEEF\n");
    volatile uint64_t *bad = (volatile uint64_t *)0xDEADBEEF;
    uint64_t v = *bad;
    (void)v;
    return 0;
}

static int builtin_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("[ INFO ] reboot: resetting\n");
    reboot_now();
    return 42;
}

static int builtin_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    term_write("[ INFO ] halt: stopping CPU\n");
    for (;;) __asm__ __volatile__("cli; hlt");
    return 42;
}

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static struct builtin builtins[] = {
    { "echo",   builtin_echo   },
    { "help",   builtin_help   },
    { "clear",  builtin_clear  },
    { "about",  builtin_about  },
    { "versi",  builtin_versi  },
    { "colors", builtin_colors },
    { "uptime", builtin_uptime },
    { "crash",  builtin_crash  },
    { "pf",     builtin_pf     },
    { "reboot", builtin_reboot },
    { "halt",   builtin_halt   },
    { 0, 0 },
};

static void print_prompt(void) {
    term_write("\033[92mberin\033[93m:root\033[90m:~\033[92m# \033[0m");
}

static void print_command_not_found(const char *name) {
    term_write("\033[91mcommand_not_found: ");
    term_write(name);
    term_write("\033[0m\n");
}

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

static void kerbash_run(void) {
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

/* ============================================================
 *  Entry
 * ============================================================ */
void kernel_main(void) {
    serial_init();

    term_set_fg(C_LGREEN);
    term_set_bg(C_BLACK);
    term_clear();

    idt_init();
    pic_remap();
    pit_init(100);
    kb_init();
    pic_unmask(0);
    pic_unmask(1);
    __asm__ __volatile__("sti");

    term_write("Berin x86_64 loaded!\n");
    term_write("Long mode: OK\n");
    term_write("\n");
    term_write(" /\\     /\\\n");
    term_write("/  \\___/  \\\n");
    term_write("(  o   o  )\n");
    term_write(" \\   \x1F   /\n");
    term_write("  \\_____/\n");
    term_write("\n");
    term_write("Berin OS v0.0.2 (x86_64)\n");
    term_write("vendor: Turker Games\n");
    term_write("\n");

    kerbash_run();

    for (;;)
        __asm__ __volatile__("cli; hlt");
}
