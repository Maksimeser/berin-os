/* Berin OS - berbash 0.1 (userland shell) */
#include "libb.h"
#define VFS_DIR  2
#define VFS_FILE 1

#define MAX_LINE 256
#define MAX_ARGS 16

static char line_buf[MAX_LINE];
static int  line_len;

static int str_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static void print_prompt(void) {
    puts("\033[92mberin\033[90m:\033[93mberbash\033[92m$ \033[0m");
}

/* ------- builtins ------- */

static int builtin_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar_(' ');
        puts(argv[i]);
    }
    putchar_('\n');
    return 0;
}

static int builtin_help(int argc, char **argv) {
    if (argc >= 2 && str_eq(argv[1], "d-")) {
        puts("berbash 0.1 - debug/destructive:\n");
        puts("  crash          trigger divide error (halts system)\n");
        return 0;
    }
    puts("berbash 0.1 (userland) - builtins:\n");
    puts("  echo <args>    print arguments\n");
    puts("  help           this message\n");
    puts("  exit           exit shell\n");
    puts("  whoami         current user\n");
    puts("  versi          versions\n");
    puts("  ls [path]      list directory\n");
    puts("  cat <path>     print file\n");
    puts("  \033[90m(for debug tools type 'help d-')\033[0m\n");
    return 0;
}

static int builtin_exit(int argc, char **argv) {
    (void)argc; (void)argv;
    return 42;
}

static int builtin_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("berbash root\n");
    return 0;
}

static int builtin_versi(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("Berin OS 0.0.2 \"Sorsha\" (x86_64)\n");
    puts("  kernel   0.0.2   x86_64, long mode\n");
    puts("  kerbash  0.2     kernel shell\n");
    puts("  berbash  0.1     userland shell\n");
    puts("  berfs    1.0     filesystem\n");
    puts("vendor: Turker Games\n");
    return 0;
}

static int builtin_ls(int argc, char **argv) {
    const char *path = (argc >= 2) ? argv[1] : "/";
    puts(path); puts(":\n");

    uint32_t pos = 0, ino;
    char name[256];
    uint8_t type;
    long r;
    while ((r = _syscall6(SYS_LISTDIR, (long)path, (long)&pos,
        (long)name, (long)&ino, (long)&type)) == 1) {
        putchar_(type == 2 ? 'd' : '-');
    putchar_(' ');
    printf("%u", ino);
    putchar_(' ');
    puts(name);
    putchar_('\n');
        }
        return 0;
}

static int builtin_cat(int argc, char **argv) {
    if (argc < 2) { puts("usage: cat <path>\n"); return 0; }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[1]);
        return 0;
    }
    char buf[128];
    long n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }
    close(fd);
    return 0;
}

/* ------- dispatch ------- */

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static int builtin_crash(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("[user] triggering divide error from ring 3...\n");
    __asm__ __volatile__(
        "movq $0, %%rax\n\t"
        "divq %%rax\n\t"
        : : : "rax", "rdx");
    return 0;
}

static struct builtin builtins[] = {
    { "echo",   builtin_echo   },
    { "help",   builtin_help   },
    { "exit",   builtin_exit   },
    { "whoami", builtin_whoami },
    { "versi",  builtin_versi  },
    { "ls",     builtin_ls     },
    { "cat",    builtin_cat    },
    { "crash",  builtin_crash  },
    { 0, 0 },
};

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

static void line_edit(void) {
    long n = read(0, line_buf, MAX_LINE - 1);
    if (n == -2) {
        puts("^C\n");
        line_len = 0;
        return;
    }
    if (n < 0) n = 0;
    if (n == 1 && line_buf[0] == 3) {
        puts("^C\n");
        line_len = 0;
        return;
    }
    if (n > 0 && line_buf[n - 1] == '\n') n--;
    line_len = (int)n;
    line_buf[line_len] = '\0';
}

void _start(void) {
    puts("berbash 0.1 (userland)\n");
    puts("type 'help' for builtins, 'exit' to quit\n\n");

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
                if (rc == 42) {
                    puts("[berbash] exit\n");
                    _exit(0);
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("\033[91mcommand_not_found: %s\033[0m\n", cmd);
        }
    }
}
