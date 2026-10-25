#ifndef BERIN_LIBB_H
#define BERIN_LIBB_H

#include <stdint.h>
#include <stddef.h>

/* Номера системных вызовов */
#define SYS_READ     0
#define SYS_WRITE    1
#define SYS_OPEN     2
#define SYS_CLOSE    3
#define SYS_STAT     4
#define SYS_LISTDIR  100
#define SYS_MKDIR    83
#define SYS_UNLINK   87
#define SYS_EXIT     60

/* Флаги для open() */
#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR   0x03
#define O_CREAT  0x04
#define O_TRUNC  0x08

struct stat_buf {
    uint32_t size;
    uint8_t  type;
};

long _syscall6(long n, long a, long b, long c, long d, long e);
long read(int fd, void *buf, size_t len);
long write(int fd, const void *buf, size_t len);
int  open(const char *path, int flags, ...);
int  close(int fd);
void _exit(int code);
int  mkdir(const char *path, int mode);
int  unlink(const char *path);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);

void printf(const char *fmt, ...);
void puts(const char *s);
void putchar_(char c);

#endif
