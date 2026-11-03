#include <stdint.h>

void _start(void) {
    const char *msg = "[user] HELLO FROM RING 3!\n";
    int len = 25;
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(1), "D"(1), "S"(msg), "d"(len)
        : "memory"
    );
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(60), "D"(0)
        : "memory"
    );
    for(;;);
}
