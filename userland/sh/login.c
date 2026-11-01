#include <stdint.h>
#include <stddef.h>

#ifdef __BERIN__
#include "libb.h"
#define ENTRY _start
#ifndef SYS_SPAWN
#define SYS_SPAWN 200
#endif
#ifndef SYS_READ_PASS
#define SYS_READ_PASS 202
#endif
#else
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define ENTRY main
#endif

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

static void read_line(char *buf, int max) {
    #ifdef __BERIN__
    long n = read(0, buf, max - 1);
    if (n > 0) {
        if (buf[n - 1] == '\n') buf[n - 1] = '\0';
        else buf[n] = '\0';
    } else {
        buf[0] = '\0';
    }
    #else
    fgets(buf, max, stdin);
    buf[strcspn(buf, "\n")] = '\0';
    #endif
}

static void read_password(char *buf, int max) {
    #ifdef __BERIN__
    _syscall6(SYS_READ_PASS, (long)buf, max, 0, 0, 0);
    #else
    fgets(buf, max, stdin);
    buf[strcspn(buf, "\n")] = '\0';
    #endif
}

#ifdef __BERIN__
void _start(void) {
    write(1, "\033[2J\033[H", 7);
    write(1, "\033[92mBerin OS 0.0.3 (x86_64)\033[0m\n", 34);
    write(1, "Copyright (c) Turker Games\n\n", 28);

    char user[32];
    char pass[32];

    while (1) {
        // Подсказка снизу (обновил текст)
        write(1, "\033[90m(login: root/root or user/user | type 'reboot' or 'shutdown' to power off)\033[0m\n", 86);

        write(1, "berin login: ", 13);

        // Очищаем буфер перед вводом
        for (int i = 0; i < 32; i++) user[i] = 0;
        read_line(user, sizeof(user));

        // Спец-команды
        if (str_eq(user, "reboot")) {
            write(1, "[ INFO ] rebooting...\n", 23);
            _syscall6(210, 0, 0, 0, 0, 0); // <--- ИЗМЕНЕНО на SYS_REBOOT
            for(;;) __asm__ __volatile__("hlt");
        }
        if (str_eq(user, "shutdown") || str_eq(user, "poweroff") || str_eq(user, "halt")) {
            write(1, "[ INFO ] shutting down...\n", 27);
            _syscall6(211, 0, 0, 0, 0, 0); // <--- ИЗМЕНЕНО на SYS_SHUTDOWN
            for(;;) __asm__ __volatile__("hlt");
        }

        write(1, "Password: ", 10);
        for (int i = 0; i < 32; i++) pass[i] = 0;
        read_password(pass, sizeof(pass));

        // ВОТ ЗДЕСЬ ТВОЙ НОВЫЙ БЛОК ПРОВЕРКИ:
        if (str_eq(user, "root") && str_eq(pass, "root")) {
            write(1, "\n\033[92m[ OK ] Welcome, root!\033[0m\n", 38);
            // Передаем uid 0 в spawn
            long pid = _syscall6(SYS_SPAWN, (long)"/bin/berbash.elf", 0, 0, 0, 0);
            if (pid > 0) { int st; _syscall6(61, pid, (long)&st, 0, 0, 0); }
        }
        else if (str_eq(user, "user") && str_eq(pass, "user")) {
            write(1, "\n\033[92m[ OK ] Welcome, user!\033[0m\n", 38);
            // Передаем uid 1000 в spawn
            long pid = _syscall6(SYS_SPAWN, (long)"/bin/berbash.elf", 1000, 0, 0, 0);
            if (pid > 0) { int st; _syscall6(61, pid, (long)&st, 0, 0, 0); }
        }
        else {
            write(1, "\n\033[91mLogin incorrect\033[0m\n\n", 27);
        }
    }
}
#else
int main(void) { return 0; }
#endif
