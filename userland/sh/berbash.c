/* Berin OS / Berony Linux - Universal berbash 0.1.2 */
#include <stdint.h>
#include <stddef.h>

#ifdef __BERIN__
#include "libb.h"
#define ENTRY _start
#define exit _exit
#ifndef SYS_MKDIR
#define SYS_MKDIR 83
#endif
#ifndef SYS_UNLINK
#define SYS_UNLINK 87
#endif
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>
#include <signal.h>
#define ENTRY main
#endif

#define MAX_LINE 256
#define MAX_ARGS 16
#define MAX_PATH 256
#define PROMPT_MAX 1024

static char cwd[MAX_PATH] = "/";
static char home[MAX_PATH] = "/";
static char username[64] = "root";
static char hostname[64] = "berin";
static char ps1[512];
static int eof_seen = 0;
static int exit_code = 0;

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

static int bb_atoi(const char *s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') { res = res * 10 + (*s - '0'); s++; }
    return res;
}

static const char *bb_strchr(const char *s, char c) {
    while (*s) {
        if (*s == c) return s;
        s++;
    }
    return 0;
}

static int bb_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void out_str(const char *s, int len) {
    #ifdef __BERIN__
    _syscall6(SYS_WRITE, 1, (long)s, len, 0, 0);
    #else
    write(1, s, len);
    #endif
}

static void out_char(char c) {
    out_str(&c, 1);
}

/* ================= ОС-ЗАВИСИМЫЙ ФЛАГ-ПАРСЕР ================= */

static unsigned parse_flags(const char *tok, const char *valid) {
    if (str_eq(tok, "--")) return 0xFFFFFFFF;
    unsigned mask = 0;
    #ifdef __BERIN__
    int len = bb_strlen(tok);
    if (len < 2 || tok[len - 1] != '-') return 0;
    for (int i = 0; i < len - 1; i++) {
        const char *p = bb_strchr(valid, tok[i]);
        if (!p) return 0;
        mask |= 1u << (p - valid);
    }
    #else
    if (tok[0] != '-' || tok[1] == '\0') return 0;
    for (const char *p = tok + 1; *p; p++) {
        const char *q = bb_strchr(valid, *p);
        if (!q) return 0;
        mask |= 1u << (q - valid);
    }
    #endif
    return mask;
}

/* ================= ОКРУЖЕНИЕ И ПУТИ ================= */

static void init_env(void) {
    #ifdef __BERIN__
    long uid = _syscall6(102, 0, 0, 0, 0, 0);
    int i = 0;

    if (uid == 0) {
        const char *u = "root";
        while (u[i]) { username[i] = u[i]; i++; } username[i] = 0;
        const char *p = "\\e[90m[\\e[93m\\u\\e[90m@\\e[92m\\h\\e[90m \\e[94m\\w\\e[90m]\\e[97m#\\e[0m ";
        i = 0; while (p[i]) { ps1[i] = p[i]; i++; } ps1[i] = 0;
    } else if (uid == 1000) {
        const char *u = ">user";
        while (u[i]) { username[i] = u[i]; i++; } username[i] = 0;
        const char *p = "\\e[90m[\\e[93m\\u\\e[90m@\\e[92m\\h\\e[90m \\e[94m\\w\\e[90m]\\e[97m$\\e[0m ";
        i = 0; while (p[i]) { ps1[i] = p[i]; i++; } ps1[i] = 0;
    } else {
        const char *u = "!admin";
        while (u[i]) { username[i] = u[i]; i++; } username[i] = 0;
        const char *p = "\\e[90m[\\e[93m\\u\\e[90m@\\e[92m\\h\\e[90m \\e[94m\\w\\e[90m]\\e[97m$\\e[0m ";
        i = 0; while (p[i]) { ps1[i] = p[i]; i++; } ps1[i] = 0;
    }

    const char *h = "/"; i = 0; while (h[i]) { home[i] = h[i]; i++; } home[i] = 0;
    i = 0; while (h[i]) { cwd[i] = h[i]; i++; } cwd[i] = 0;
    const char *hn = "berin"; i = 0; while (hn[i]) { hostname[i] = hn[i]; i++; } hostname[i] = 0;
    #else
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username) - 1);
        strncpy(home, pw->pw_dir, sizeof(home) - 1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, home);
    strcpy(hostname, "berony");
    strcpy(ps1, "\\e[90m[\\e[93m\\u\\e[90m@\\e[92m\\h\\e[90m \\e[94m\\w\\e[90m]\\e[97m\\$\\e[0m ");
    #endif
}

static void make_abs(const char *path, char *out) {
    if (!path || !*path) {
        int i = 0; while (cwd[i]) { out[i] = cwd[i]; i++; } out[i] = 0;
        return;
    }
    char tmp[MAX_PATH * 2];
    int i = 0;

    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        int h = 0;
        while (home[h] && i < MAX_PATH * 2 - 1) tmp[i++] = home[h++];
        path++;
    } else if (path[0] != '/') {
        int c = 0;
        while (cwd[c] && i < MAX_PATH * 2 - 1) tmp[i++] = cwd[c++];
        if (i > 0 && tmp[i - 1] != '/') tmp[i++] = '/';
    }

    int p = 0;
    while (path[p] && i < MAX_PATH * 2 - 1) tmp[i++] = path[p++];
    tmp[i] = '\0';

    char *components[128];
    int comp_count = 0;
    char *p_ptr = tmp;

    while (*p_ptr) {
        while (*p_ptr == '/') p_ptr++;
        if (!*p_ptr) break;
        char *start = p_ptr;
        while (*p_ptr && *p_ptr != '/') p_ptr++;
        if (*p_ptr) *p_ptr++ = '\0';

        if (str_eq(start, ".")) {
        } else if (str_eq(start, "..")) {
            if (comp_count > 0) comp_count--;
        } else {
            if (comp_count < 128) components[comp_count++] = start;
        }
    }

    int o = 0;
    out[o++] = '/';
    for (int c = 0; c < comp_count; c++) {
        int l = 0;
        while (components[c][l]) {
            if (o < MAX_PATH - 1) out[o++] = components[c][l];
            l++;
        }
        if (c < comp_count - 1 && o < MAX_PATH - 1) out[o++] = '/';
    }
    if (o == 0) out[o++] = '/';
    out[o] = '\0';
}

/* ================= ПРОМПТ (PS1) ================= */

static void expand_prompt(const char *in, char *out, int outsz) {
    int o = 0, i = 0;
    while (in[i] && o < outsz - 1) {
        char ch = in[i];
        if (ch != '\\') { out[o++] = ch; i++; continue; }
        i++;
        if (!in[i]) break;
        char c = in[i++];
        switch (c) {
            case 'u': { const char *p = username; while (*p && o < outsz-1) out[o++] = *p++; break; }
            case 'h': { const char *p = hostname; while (*p && o < outsz-1) out[o++] = *p++; break; }
            case 'w': {
                int hl = bb_strlen(home);
                int match = (hl > 1) && (home[0] != '/');
                if (match) {
                    int c = 0;
                    while (c < hl && cwd[c] == home[c]) c++;
                    if (c < hl) match = 0;
                }
                if (match) {
                    out[o++] = '~';
                    const char *p = cwd + hl;
                    while (*p && o < outsz-1) out[o++] = *p++;
                } else {
                    const char *p = cwd;
                    while (*p && o < outsz-1) out[o++] = *p++;
                }
                break;
            }
            case '$': out[o++] = str_eq(username, "root") ? '#' : '$'; break;
            case 'e': out[o++] = 0x1b; break;
            case '\\': out[o++] = '\\'; break;
            default: out[o++] = c; break;
        }
    }
    out[o] = '\0';
}

static void print_prompt(void) {
    char expanded[PROMPT_MAX];
    expand_prompt(ps1, expanded, sizeof(expanded));
    out_str(expanded, bb_strlen(expanded));
}

/* ================= ВВОД/ВЫВОД ================= */

static char line_buf[MAX_LINE];
static int  line_len;

#ifndef __BERIN__
volatile sig_atomic_t ctrl_c_pressed = 0;
static void handle_sigint(int sig) {
    (void)sig;
    ctrl_c_pressed = 1;
    out_str("^C\n", 3);
}
#endif

static void line_edit(void) {
    #ifdef __BERIN__
    long n = read(0, line_buf, MAX_LINE - 1);
    if (n == -2) { out_str("^C\n", 3); line_len = 0; return; }
    if (n <= 0)  { eof_seen = 1; line_len = 0; return; }
    if (line_buf[n - 1] == '\n') n--;
    line_len = (int)n; line_buf[line_len] = '\0';
    #else
    if (fgets(line_buf, MAX_LINE, stdin) == NULL) {
        if (errno == EINTR || ctrl_c_pressed) { clearerr(stdin); ctrl_c_pressed = 0; line_len = 0; return; }
        eof_seen = 1; line_len = 0; return;
    }
    if (ctrl_c_pressed) { clearerr(stdin); ctrl_c_pressed = 0; line_len = 0; return; }
    line_len = strlen(line_buf);
    if (line_len > 0 && line_buf[line_len - 1] == '\n') line_len--;
    else { int c; while ((c = getchar()) != '\n' && c != EOF) ; }
    line_buf[line_len] = '\0';
    #endif
}

static int tokenize(char *s, char **argv, int max) {
    int argc = 0;
    while (*s && argc < max) {
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        if (!*s) break;
        char quote = 0;
        argv[argc++] = s;
        char *dst = s;
        while (*s) {
            char ch = *s;
            if (quote) {
                if (ch == quote) { quote = 0; s++; continue; }
                *dst++ = *s++; continue;
            }
            if (ch == '\\') { s++; *dst++ = *s ? *s++ : '\\'; continue; }
            if (ch == '"' || ch == '\'') { quote = ch; s++; continue; }
            if (ch == ' ' || ch == '\t' || ch == '\n') break;
            *dst++ = *s++;
        }
        if (*s) s++;
        *dst = '\0';
    }
    return argc;
}

/* ================= BUILTINS ================= */

static int builtin_echo(int argc, char **argv) {
    int no_nl = 0;
    int start = 1;
    #ifdef __BERIN__
    if (argc > 1 && str_eq(argv[1], "n-")) { no_nl = 1; start = 2; }
    #else
    if (argc > 1 && str_eq(argv[1], "-n")) { no_nl = 1; start = 2; }
    #endif

    int redirect = 0;
    int out_fd = 1;
    char abs_path[MAX_PATH];

    if (argc >= start + 2 && str_eq(argv[argc - 2], ">")) {
        redirect = 1;
        const char *filename = argv[argc - 1];
        make_abs(filename, abs_path);
        out_fd = open(abs_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) { out_str("not_found\n", 10); return 0; }
        argc -= 2;
    }

    for (int i = start; i < argc; i++) {
        if (i > start) {
            #ifdef __BERIN__
            if (redirect) _syscall6(SYS_WRITE, out_fd, (long)" ", 1, 0, 0); else out_str(" ", 1);
            #else
            if (redirect) write(out_fd, " ", 1); else out_str(" ", 1);
            #endif
        }
        #ifdef __BERIN__
        if (redirect) _syscall6(SYS_WRITE, out_fd, (long)argv[i], bb_strlen(argv[i]), 0, 0);
        else out_str(argv[i], bb_strlen(argv[i]));
        #else
        if (redirect) write(out_fd, argv[i], strlen(argv[i]));
        else out_str(argv[i], strlen(argv[i]));
        #endif
    }

    if (!no_nl) {
        #ifdef __BERIN__
        if (redirect) _syscall6(SYS_WRITE, out_fd, (long)"\n", 1, 0, 0); else out_str("\n", 1);
        #else
        if (redirect) write(out_fd, "\n", 1); else out_str("\n", 1);
        #endif
    }

    if (redirect) {
        close(out_fd);
        out_str("[ OK ] written\n", bb_strlen("[ OK ] written\n"));
    }
    return 0;
}

static int builtin_su(int argc, char **argv) {
    (void)argc; (void)argv;
    #ifdef __BERIN__
    long r = _syscall6(103, 0, 0, 0, 0, 0);
    if (r == 0) {
        out_str("[ OK ] Now you are root!\n", 25);
    } else {
        out_str("[ FAIL ] Access denied\n", 23);
    }
    #endif
    return 0;
}

static int builtin_help(int argc, char **argv) {
    (void)argc; (void)argv;
    out_str("berbash 0.1.2 - builtins:\n", bb_strlen("berbash 0.1.2 - builtins:\n"));
    out_str("  echo, help, exit, whoami, clear, versi, uname\n", bb_strlen("  echo, help, exit, whoami, clear, versi, uname\n"));
    out_str("  lst, show, go, path, del, new, mkdir, stat, head, tail\n", bb_strlen("  lst, show, go, path, del, new, mkdir, stat, head, tail\n"));
    return 0;
}

static int builtin_exit(int argc, char **argv) {
    exit_code = (argc > 1) ? bb_atoi(argv[1]) : 0;
    return 42;
}

static int builtin_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(username, bb_strlen(username));
    out_str("\n", 1);
    return 0;
}

static int builtin_versi(int argc, char **argv) {
    (void)argc; (void)argv;
    #ifdef __BERIN__
    out_str("Berin OS 0.0.3 x86_64\n", bb_strlen("Berin OS 0.0.3 x86_64\n"));
    #else
    out_str("BeronyOS 0.0.1 x86_64\n", bb_strlen("BeronyOS 0.0.1 x86_64\n"));
    #endif
    return 0;
}

static int builtin_uname(int argc, char **argv) {
    (void)argc; (void)argv;
    #ifdef __BERIN__
    out_str("Berin OS 0.0.3 x86_64\n", bb_strlen("Berin OS 0.0.3 x86_64\n"));
    #else
    out_str("BeronyOS 0.0.1 x86_64\n", bb_strlen("BeronyOS 0.0.1 x86_64\n"));
    #endif
    return 0;
}

static int builtin_ls(int argc, char **argv) {
    int show_all = 0;
    int long_fmt = 0;
    #ifdef __BERIN__
    (void)long_fmt;
    #endif
    const char *path = NULL;
    int i = 1;

    while (i < argc) {
        unsigned f = parse_flags(argv[i], "la");
        if (f == 0xFFFFFFFF) { i++; break; }
        if (!f) break;
        if (f & 1) long_fmt = 1;
        if (f & 2) show_all = 1;
        i++;
    }
    if (i < argc) path = argv[i];

    char abs_path[MAX_PATH];
    if (path && str_eq(path, "*")) make_abs(".", abs_path);
    else if (path) make_abs(path, abs_path);
    else make_abs(".", abs_path);

    out_str(abs_path, bb_strlen(abs_path)); out_str(":\n", 2);

    #ifdef __BERIN__
    (void)long_fmt;
    uint32_t pos = 0, ino;
    char name[256];
    uint8_t type;
    while (_syscall6(SYS_LISTDIR, (long)abs_path, (long)&pos, (long)name, (long)&ino, (long)&type) == 1) {
        if (!show_all && name[0] == '.') continue;
        out_char(type == 2 ? 'd' : '-');
        out_char(' ');
        char num[16]; int n = 0; uint32_t v = ino; if (v == 0) num[n++] = '0'; while (v) { num[n++] = '0' + v%10; v/=10; }
        while (n) out_char(num[--n]);
        out_char(' ');
        out_str(name, bb_strlen(name));
        out_char('\n');
    }
    #else
    DIR *d = opendir(abs_path);
    if (!d) { out_str("not_found\n", 10); return 0; }
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (!show_all && dir->d_name[0] == '.') continue;
        out_str(dir->d_name, strlen(dir->d_name));
        out_char('\n');
    }
    closedir(d);
    #endif
    return 0;
}

static int builtin_cat(int argc, char **argv) {
    if (argc < 2) { out_str("usage: show <file>\n", 18); return 0; }
    for (int i = 1; i < argc; i++) {
        char abs_path[MAX_PATH];
        make_abs(argv[i], abs_path);
        int fd = open(abs_path, O_RDONLY, 0);
        if (fd < 0) { out_str("not_found\n", 10); continue; }
        char buf[128]; long n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) out_str(buf, n);
        close(fd);
    }
    return 0;
}

static int builtin_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    out_str(cwd, bb_strlen(cwd));
    out_char('\n');
    return 0;
}

static int builtin_cd(int argc, char **argv) {
    const char *path = (argc < 2) ? home : argv[1];
    char new_abs[MAX_PATH];
    make_abs(path, new_abs);

    #ifdef __BERIN__
    uint32_t pos = 0, ino; char name[256]; uint8_t type;
    if (_syscall6(SYS_LISTDIR, (long)new_abs, (long)&pos, (long)name, (long)&ino, (long)&type) < 0) {
        out_str("not_found\n", 10); return 0;
    }
    #else
    if (chdir(new_abs) < 0) { out_str("not_found\n", 10); return 0; }
    #endif
    int i = 0; while (new_abs[i]) { cwd[i] = new_abs[i]; i++; } cwd[i] = 0;
    return 0;
}

static int builtin_stat(int argc, char **argv) {
    if (argc < 2) { out_str("usage: stat <path>\n", 18); return 0; }
    char abs_path[MAX_PATH];
    make_abs(argv[1], abs_path);
    #ifdef __BERIN__
    struct stat_buf st;
    if (_syscall6(SYS_STAT, (long)abs_path, (long)&st, 0, 0, 0) < 0) { out_str("not_found\n", 10); return 0; }
    out_str("Size: ", bb_strlen("Size: ")); char num[16]; int n=0; uint32_t v=st.size; if(!v)num[n++]='0'; while(v){num[n++]='0'+v%10;v/=10;} while(n)out_char(num[--n]); out_str(" bytes\n", bb_strlen(" bytes\n"));
    #else
    struct stat st;
    if (stat(abs_path, &st) < 0) { out_str("not_found\n", 10); return 0; }
    printf("Size: %llu bytes\n", (unsigned long long)st.st_size);
    #endif
    return 0;
}

static int builtin_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    out_str("\033[2J\033[H", 7);
    return 0;
}

static int builtin_mkdir(int argc, char **argv) {
    int p_flag = 0;
    const char *path = NULL;
    int i = 1;

    while (i < argc) {
        unsigned f = parse_flags(argv[i], "p");
        if (f == 0xFFFFFFFF) { i++; break; }
        if (!f) break;
        p_flag = 1;
        i++;
    }
    if (i < argc) path = argv[i];
    if (!path) { out_str("usage: mkdir <path>\n", bb_strlen("usage: mkdir <path>\n")); return 0; }

    char abs_path[MAX_PATH];
    make_abs(path, abs_path);

    if (p_flag) {
        char tmp[MAX_PATH]; int c = 0; while(abs_path[c]){tmp[c]=abs_path[c];c++;} tmp[c]=0;
        for (char *p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                #ifdef __BERIN__
                _syscall6(SYS_MKDIR, (long)tmp, 0755, 0, 0, 0);
                #else
                mkdir(tmp, 0755);
                #endif
                *p = '/';
            }
        }
    }
    #ifdef __BERIN__
    long r = _syscall6(SYS_MKDIR, (long)abs_path, 0755, 0, 0, 0);
    #else
    long r = mkdir(abs_path, 0755);
    #endif
    if (r < 0) out_str("[ FAIL ] mkdir\n", bb_strlen("[ FAIL ] mkdir\n"));
    else out_str("[ OK ] created\n", bb_strlen("[ OK ] created\n"));
    return 0;
}

static int builtin_rm(int argc, char **argv) {
    const char *path = NULL;
    int i = 1;
    while (i < argc) {
        unsigned f = parse_flags(argv[i], "rf");
        if (f == 0xFFFFFFFF) { i++; break; }
        if (!f) break;
        i++;
    }
    if (i < argc) path = argv[i];
    if (!path) { out_str("usage: del <path>\n", bb_strlen("usage: del <path>\n")); return 0; }

    char abs_path[MAX_PATH];
    make_abs(path, abs_path);

    #ifdef __BERIN__
    long r = _syscall6(SYS_UNLINK, (long)abs_path, 0, 0, 0, 0);
    if (r < 0) out_str("[ FAIL ] not_found\n", bb_strlen("[ FAIL ] not_found\n"));
    else       out_str("[ OK ] deleted\n", bb_strlen("[ OK ] deleted\n"));
    #else
    if (unlink(abs_path) < 0) out_str("[ FAIL ] not_found\n", bb_strlen("[ FAIL ] not_found\n"));
    else                      out_str("[ OK ] deleted\n", bb_strlen("[ OK ] deleted\n"));
    #endif
    return 0;
}

static int builtin_touch(int argc, char **argv) {
    if (argc < 2) { out_str("usage: new <path>\n", bb_strlen("usage: new <path>\n")); return 0; }
    char abs_path[MAX_PATH];
    make_abs(argv[1], abs_path);
    int fd = open(abs_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) out_str("[ FAIL ] not_found\n", bb_strlen("[ FAIL ] not_found\n"));
    else { close(fd); out_str("[ OK ] created\n", bb_strlen("[ OK ] created\n"));}
    return 0;
}

static int builtin_head(int argc, char **argv) {
    (void)argc; (void)argv;
    out_str("head stub\n", bb_strlen("head stub\n"));
    return 0;
}

static int builtin_tail(int argc, char **argv) {
    (void)argc; (void)argv;
    out_str("tail stub\n", bb_strlen("tail stub\n"));
    return 0;
}

struct builtin {
    const char *name;
    int (*fn)(int argc, char **argv);
};

static int builtin_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    #ifdef __BERIN__
    _syscall6(210, 0, 0, 0, 0, 0);
    #endif
    return 0;
}
static int builtin_logoff(int argc, char **argv) {
    (void)argc; (void)argv;
    #ifdef __BERIN__
    _syscall6(211, 0, 0, 0, 0, 0);
    #endif
    return 0;
}

static int builtin_run(int argc, char **argv) {
    if (argc < 2) { out_str("usage: run <path>\n", 18); return 0; }
    char abs_path[MAX_PATH];
    make_abs(argv[1], abs_path);

    #ifdef __BERIN__
    long pid = _syscall6(200, (long)abs_path, 0, 0, 0, 0); // sys_spawn
    if (pid < 0) {
        out_str("[ FAIL ] run\n", 14);
    } else {
        int status = 0;
        _syscall6(61, pid, (long)&status, 0, 0, 0);
        out_str("[ OK ] process finished\n", 24);
    }
    #else
    pid_t pid = fork();
    if (pid == 0) { execvp(argv[1], &argv[1]); exit(1); }
    else if (pid > 0) waitpid(pid, NULL, 0);
    #endif
    return 0;
}

static struct builtin builtins[] = {
    { "help",    builtin_help    },  { "clear",   builtin_clear   },
    { "exit",    builtin_exit    },  { "echo",    builtin_echo    },
    { "reboot",  builtin_reboot  },  { "logoff",  builtin_logoff  },
    { "whoami",  builtin_whoami  },  { "versi",   builtin_versi   },
    { "uname",   builtin_uname   },
    { "lst",     builtin_ls      },  { "show",    builtin_cat     },
    { "go",      builtin_cd      },  { "path",    builtin_pwd     },
    { "del",     builtin_rm      },  { "new",     builtin_touch   },
    { "mkdir",   builtin_mkdir   },  { "stat",    builtin_stat    },
    { "head",    builtin_head    },  { "tail",    builtin_tail    },
    { "ls",      builtin_ls      },  { "cat",     builtin_cat     },
    { "cd",      builtin_cd      },  { "pwd",     builtin_pwd     },
    { "rm",      builtin_rm      },  { "touch",   builtin_touch   },
    {"run",      builtin_run     },  { "su",      builtin_su      },
    { 0, 0 },
};

static int run_command(int argc, char **argv) {
    if (argc == 0) return 0;
    const char *cmd = argv[0];
    for (int i = 0; builtins[i].name; i++) {
        if (str_eq(builtins[i].name, cmd)) {
            return builtins[i].fn(argc, argv);
        }
    }
    #ifdef __BERIN__
    out_str("command_not_found\n", 18);
    return 0;
    #else
    pid_t pid = fork();
    if (pid == 0) { execvp(cmd, argv); exit(1); }
    else if (pid > 0) waitpid(pid, NULL, 0);
    return 0;
    #endif
}

#ifdef __BERIN__
void _start(void) {
    init_env();
    out_str("Berbash 0.1.2\n", bb_strlen("Berbash 0.1.2\n"));
    for (;;) {
        print_prompt();
        line_edit();
        if (eof_seen) break;
        if (line_len == 0) continue;
        char *argv[MAX_ARGS];
        int argc = tokenize(line_buf, argv, MAX_ARGS);
        int rc = run_command(argc, argv);
        if (rc == 42) { out_str("exit\n", 5); _exit(exit_code); }
        if (rc == 100) _exit(100);
        if (rc == 101) _exit(101);
    }
    _exit(0);
}
#else
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    init_env();
    signal(SIGINT, handle_sigint);
    out_str("Berbash 0.1.2 (Linux)\n", bb_strlen("Berbash 0.1.2 (Linux)\n"));
    for (;;) {
        print_prompt();
        line_edit();
        if (eof_seen) break;
        if (line_len == 0) continue;
        char *c_argv[MAX_ARGS];
        int c_argc = tokenize(line_buf, c_argv, MAX_ARGS);
        int rc = run_command(c_argc, c_argv);
        if (rc == 42) exit(exit_code);
    }
    return 0;
}
#endif
