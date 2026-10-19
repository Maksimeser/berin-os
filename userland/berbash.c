/* Berin OS - berbash 0.1 (userland shell) */
#include "libb.h"
#define VFS_DIR  2
#define VFS_FILE 1

#define MAX_LINE 256
#define MAX_ARGS 16

static char cwd[256] = "/";
static char home[256] = "/";

static void make_abs(const char *path, char *out) {
    if (path[0] == '/') {
        int i = 0;
        while (path[i] && i < 255) { out[i] = path[i]; i++; }
        out[i] = '\0';
        return;
    }
    int i = 0, j = 0;
    while (cwd[i] && i < 255) { out[i] = cwd[i]; i++; }
    if (i > 0 && out[i - 1] != '/') out[i++] = '/';
    while (path[j] && i < 255) out[i++] = path[j++];
    out[i] = '\0';
}

static char line_buf[MAX_LINE];
static int  line_len;

static int str_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static void print_prompt(void) {
    puts("\033[90m[\033[93m!admin\033[90m@\033[92mberin\033[90m \033[94m");
    puts(cwd);
    puts("\033[90m]\033[97m# \033[0m");
}

/* ------- builtins ------- */

static int builtin_echo(int argc, char **argv) {
    if (argc < 2) { putchar_('\n'); return 0; }

    int redirect = 0;
    int out_fd = 1;
    char abs_path[256];

    if (argc >= 3 && str_eq(argv[argc - 2], ">")) {
        redirect = 1;
        const char *filename = argv[argc - 1];
        make_abs(filename, abs_path);

        out_fd = open(abs_path, O_WRONLY | O_CREAT | O_TRUNC);
        if (out_fd < 0) {
            printf("\033[91mnot_found: %s\033[0m\n", filename);
            return 0;
        }
        argc -= 2;
    }

    for (int i = 1; i < argc; i++) {
        if (i > 1) write(out_fd, " ", 1);
        write(out_fd, argv[i], strlen(argv[i]));
    }
    write(out_fd, "\n", 1);

    if (redirect) close(out_fd);
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
    puts("  clear          clear screen\n");
    puts("  reboot         reboot machine\n");
    puts("  wheiam         detailed info (user, cwd, uid)\n");
    puts("  logoff         shut down (ACPI)\n");
    puts("  mkdir <path>   create directory\n");
    puts("  touch <path>   create empty file\n");
    puts("  run <path>     run program in child process\n");
    puts("  versi          versions\n");
    puts("  stat <path>    file info (size, type)\n");
    puts("  head [n- N] <f>print first N lines (default 10)\n");
    puts("  tail <path>    print last 10 lines\n");
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
    char abs_path[256];
    if (argc >= 2) make_abs(argv[1], abs_path);
    else { int i = 0; while (cwd[i] && i < 255) { abs_path[i] = cwd[i]; i++; } abs_path[i] = '\0'; }
    puts(abs_path); puts(":\n");
    uint32_t pos = 0, ino;
    char name[256];
    uint8_t type;
    long r;
    while ((r = _syscall6(SYS_LISTDIR, (long)abs_path, (long)&pos,
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
    char abs_path[256];
    make_abs(argv[1], abs_path);
    int fd = open(abs_path, O_RDONLY);
    if (fd < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[1]);
        return 0;
    }
    char buf[128];
    long n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) write(1, buf, n);
    close(fd);
    return 0;
}

/* ------- dispatch ------- */

static int builtin_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    puts(cwd);
    putchar_('\n');
    return 0;
}

static int builtin_cd(int argc, char **argv) {
    const char *path;
    if (argc < 2) {
        path = home;
    } else {
        path = argv[1];
    }

    if (str_eq(path, "..")) {
        int len = 0;
        while (cwd[len]) len++;
        if (len <= 1) { cwd[0] = '/'; cwd[1] = '\0'; return 0; }
        while (len > 0 && cwd[len - 1] != '/') len--;
        if (len <= 1) { cwd[0] = '/'; cwd[1] = '\0'; }
        else { cwd[len - 1] = '\0'; }
        return 0;
    }

    char new_abs[256];
    make_abs(path, new_abs);

    if (str_eq(path, "..")) {
        int len = 0;
        while (cwd[len]) len++;
        if (len <= 1) {
            cwd[0] = '/'; cwd[1] = '\0';
            return 0;
        }
        while (len > 0 && cwd[len - 1] != '/') len--;
        if (len <= 1) { cwd[0] = '/'; cwd[1] = '\0'; }
        else { cwd[len - 1] = '\0'; }
        return 0;
    }

    make_abs(path, new_abs);

    uint32_t pos = 0, ino; char name[256]; uint8_t type;
    long r = _syscall6(SYS_LISTDIR, (long)new_abs, (long)&pos, (long)name, (long)&ino, (long)&type);
    if (r < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", path);
        return 0;
    }

    int i = 0;
    while (new_abs[i] && i < 255) { cwd[i] = new_abs[i]; i++; }
    cwd[i] = '\0';
    return 0;
}

/* --- stat --- */
static int builtin_stat(int argc, char **argv) {
    if (argc < 2) { puts("usage: stat <path>\n"); return 0; }
    char abs_path[256];
    make_abs(argv[1], abs_path);

    struct stat_buf st;
    long r = _syscall6(SYS_STAT, (long)abs_path, (long)&st, 0, 0, 0);
    if (r < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[1]);
        return 0;
    }

    puts("File: "); puts(abs_path); puts("\n");
    printf("Size: %u bytes\n", st.size);
    printf("Type: %s\n", st.type == 2 ? "directory" : "regular file");
    return 0;
}

/* --- head --- */
static int builtin_head(int argc, char **argv) {
    if (argc < 2) { puts("usage: head [-n lines] <file>\n"); return 0; }

    int lines = 10;
    int argi = 1;

    /* Очень простой парсер флага -n (задом наперёд, как у нас принято: -n или просто число) */
    if (argc >= 4 && str_eq(argv[1], "n-")) {
        lines = 0;
        const char *num = argv[2];
        for (int i = 0; num[i] >= '0' && num[i] <= '9'; i++) {
            lines = lines * 10 + (num[i] - '0');
        }
        argi = 3;
    }

    char abs_path[256];
    make_abs(argv[argi], abs_path);

    int fd = open(abs_path, O_RDONLY);
    if (fd < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[argi]);
        return 0;
    }

    char c;
    int cnt = 0;
    while (read(fd, &c, 1) == 1) {
        putchar_(c);
        if (c == '\n') {
            cnt++;
            if (cnt >= lines) break;
        }
    }
    close(fd);
    return 0;
}

/* --- tail --- */
static int builtin_tail(int argc, char **argv) {
    if (argc < 2) { puts("usage: tail <file>\n"); return 0; }
    char abs_path[256];
    make_abs(argv[1], abs_path);
    int fd = open(abs_path, O_RDONLY);
    if (fd < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[1]);
        return 0;
    }

    /* Читаем весь файл (макс 64 КБ по лимиту VFS) */
    static char buf[65536];
    long n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    /* Ищем последние 10 строк */
    int lines = 10;
    long start = n;
    int cnt = 0;
    if (n > 0 && buf[n - 1] == '\n') { start = n - 1; }
    while (start > 0) {
        if (buf[start - 1] == '\n') {
            cnt++;
            if (cnt >= lines) break;
        }
        start--;
    }
    write(1, buf + start, n - start);
    return 0;
}

static int builtin_mkdir(int argc, char **argv) {
    if (argc < 2) { puts("usage: mkdir <path>\n"); return 0; }
    char abs_path[256];
    make_abs(argv[1], abs_path);

    long r = mkdir(abs_path, 0755);
    if (r == -2) {
        printf("\033[91malready_exists: %s\033[0m\n", argv[1]);
    } else if (r < 0) {
        printf("\033[91mnot_found (parent dir?): %s\033[0m\n", argv[1]);
    }
    return 0;
}

static int builtin_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    write(1, "\033[2J\033[H", 7);
    return 0;
}

static int builtin_wheiam(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("user:   root\n");
    puts("uid:    0\n");
    puts("gid:    0\n");
    puts("home:   /\n");
    puts("home:   "); puts(home); puts("\n");
    puts("cwd:    "); puts(cwd); puts("\n");
    puts("shell:  /bin/berbash\n");
    puts("vendor: Turker Games\n");
    return 0;
}

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static int builtin_run(int argc, char **argv) {
    if (argc < 2) { puts("usage: run <path>\n"); return 0; }
    char abs_path[256];
    make_abs(argv[1], abs_path);

    long r = spawn(abs_path);
    if (r < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[1]);
    } else {
        printf("[berbash] process exited with code %d\n", (int)r);
    }
    return 0;
}

static int builtin_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("rebooting...\n");
    _exit(100);
    return 0;
}

static int builtin_logoff(int argc, char **argv) {
    (void)argc; (void)argv;
    puts("shutting down...\n");
    _exit(101);
    return 0;
}

static int builtin_rm(int argc, char **argv) {
    if (argc < 2) { puts("usage: rm <path>\n"); return 0; }
    char abs_path[256];
    make_abs(argv[1], abs_path);

    // syscall 87 = unlink
    long r = _syscall6(87, (long)abs_path, 0, 0, 0, 0);
    if (r < 0) {
        printf("\033[91mnot_found: %s\033[0m\n", argv[1]);
    }
    return 0;
}

static int builtin_touch(int argc, char **argv) {
    if (argc < 2) { puts("usage: touch <path>\n"); return 0; }
    char abs_path[256];
    make_abs(argv[1], abs_path);
    int fd = open(abs_path, O_RDWR | O_CREAT);
    if (fd < 0) {
        printf("\033[91mnot_found (parent dir?): %s\033[0m\n", argv[1]);
    } else {
        close(fd);
    }
    return 0;
}

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
    { "touch",  builtin_touch  },
    { "pwd",    builtin_pwd    },
    { "cd",     builtin_cd     },
    { "mkdir",  builtin_mkdir  },
    { "wheiam", builtin_wheiam },
    { "cat",    builtin_cat    },
    { "reboot", builtin_reboot },
//  { "run",    builtin_run    },
    { "logoff", builtin_logoff },
    { "rm",     builtin_rm     },
    { "stat",   builtin_stat   },
    { "clear",  builtin_clear  },
    { "head",   builtin_head   },
    { "tail",   builtin_tail   },
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
