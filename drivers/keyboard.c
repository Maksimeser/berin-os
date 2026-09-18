/* Berin OS - PS/2 keyboard (scancode set 1, US layout, Shift support) */

#include "keyboard.h"
#include "io.h"

#define KB_DATA   0x60
#define KB_STATUS 0x64
#define BUF_SIZE  256

static const char map[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ',
};

static const char map_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ',
};

static int shift_down;
static char buf[BUF_SIZE];
static volatile int head, tail;

void kb_init(void) {
    while (inb(KB_STATUS) & 0x01) (void)inb(KB_DATA);   /* flush */
        head = tail = 0;
    shift_down = 0;
}

void kb_handler(void) {
    uint8_t sc = inb(KB_DATA);

    if (sc & 0x80) {                       /* key release */
        uint8_t code = sc & 0x7F;
        if (code == 0x2A || code == 0x36) shift_down = 0;
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift_down = 1; return; }

    char c = shift_down ? map_shift[sc] : map[sc];
    if (!c) return;

    int next = (head + 1) % BUF_SIZE;
    if (next != tail) {                    /* drop if full */
        buf[head] = c;
        head = next;
    }
}

int kb_ready(void) { return head != tail; }

char kb_getchar(void) {
    while (head == tail) __asm__ __volatile__("hlt");
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
