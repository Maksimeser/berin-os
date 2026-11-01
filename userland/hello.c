#include <stdint.h>

static void print(const char *str) {
    int len = 0;
    while (str[len]) len++;
    __asm__ __volatile__("int $0x80" :: "a"(1), "D"(1), "S"(str), "d"(len));
}

void _start(void) {
    long argc;
    char **argv;

    __asm__ __volatile__(
        "mov %%rsp, %0\n\t"
    "mov (%0), %1\n\t"
    "add $8, %0\n\t"
    : "=r"(argv), "=r"(argc)
    );

    print("[user] Hello from Ring 3!\n");
    print("[user] argc = ");

    char ac_buf[2] = { '0' + argc, '\n', };
    __asm__ __volatile__("int $0x80" :: "a"(1), "D"(1), "S"(ac_buf), "d"(2));

    if (argc > 1) {
        print("[user] argv[1] = ");
        print(argv[1]);
        print("\n");
    }

    __asm__ __volatile__("int $0x80" :: "a"(60), "D"(0)); // exit(0)
    for(;;);
}
