#include <stdint.h>
#include "libb.h"

void _start(void) {
    // Достаем argc и argv из стека (ядро их туда положило)
    uint64_t *stack_ptr;
    __asm__ __volatile__ ("mov %%rsp, %0" : "=r"(stack_ptr));
    int argc = (int)stack_ptr[0];
    char **argv = (char **)&stack_ptr[1];

    _syscall6(SYS_WRITE, 1, (long)"[ HELLO ] Program started!\n", 27, 0, 0);

    for (int i = 0; i < argc; i++) {
        _syscall6(SYS_WRITE, 1, (long)"argv[", 5, 0, 0);
        char num[2] = {'0' + i, 0};
        _syscall6(SYS_WRITE, 1, (long)num, 1, 0, 0);
        _syscall6(SYS_WRITE, 1, (long)"] = ", 4, 0, 0);

        int len = 0;
        while (argv[i][len]) len++; // strlen
        _syscall6(SYS_WRITE, 1, (long)argv[i], len, 0, 0);
        _syscall6(SYS_WRITE, 1, (long)"\n", 1, 0, 0);
    }
    _exit(0);
}
