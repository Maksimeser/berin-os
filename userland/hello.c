#include <stdint.h>

void _start(void) {
    const char *msg = "[user] HELLO START!\n";
    int len = 0;
    while (msg[len]) len++;

    // sys_write(1, msg, len)
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(1), "D"(1), "S"(msg), "d"(len)
        : "memory"
    );

    // sys_exit(0)
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(60), "D"(0)
        : "memory"
    );

    for(;;);
}
