/* Berin OS - berbash 0.1
 * Minimal shell: echo, help, clear, logoff.
 * Input: serial (COM1). Output: VGA + serial.
 */

#include <stdint.h>
#include <stddef.h>
#include "serial.h"

extern void term_write(const char *s);
extern void term_putchar(char c);
extern void term_set_fg(uint8_t fg);

#define MAX_LINE 256
#define MAX_ARGS 16

/* VGA color indexes (subset) */
#define C_GREEN   10
#define C_YELLOW  14
#define C_GRAY     7
#define C_RED     12

static char line_buf[MAX_LINE];
static int  line_len;

/* ---------------- builtins ---------------- */

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
    /* ACPI shutdown - covers Q35 and i440fx QEMU machines */
    outw(0x604,  0x2000);
    outw(0x4004, 0x2000);
    return 42;   /* signal to shell: exit loop -> kernel halts */
}

/* ---------------- dispatch table ---------------- */

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static struct builtin builtins[] = {
    { "echo",   builtin_echo   },
    { "help",   builtin_help   },
    { "clear",  builtin_clear  },
    { "logoff", builtin_logoff },
    { NULL, NULL },
};

/* ---------------- helpers ---------------- */

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

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

/* ---------------- line editor ---------------- */

static void line_edit(void) {
    line_len = 0;
    for (;;) {
        char c = serial_read_char();

        if (c == '\r' || c == '\n') {
            term_putchar('\n');
            line_buf[line_len] = '\0';
            return;
        }

        if (c == 0x7F || c == '\b') {   /* DEL / BS */
            if (line_len > 0) {
                line_len--;
                term_putchar('\b');
                term_putchar(' ');
                term_putchar('\b');
            }
            continue;
        }

        if ((unsigned char)c < 0x20) continue;   /* other control chars */

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

/* ---------------- entry ---------------- */

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
                if (rc == 42) return;   /* logoff */
                    found = 1;
                break;
            }
        }
        if (!found) print_command_not_found(cmd);
    }
}
