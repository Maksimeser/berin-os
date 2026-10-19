/* Berin OS - PS/2 keyboard (x86_64), scancode set 1 + modifiers */
#include "keyboard.h"
#include "io.h"

#define KB_DATA   0x60
#define KB_STATUS 0x64
#define BUF_SIZE  256

static const char map[128] = {
    0,   27,  '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0,   ' ',
};
static const char map_shift[128] = {
    0,   27,  '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0,   ' ',
};

static int shift_down, ctrl_down, alt_down;
static char buf[BUF_SIZE];
static volatile int head, tail;

void kb_init(void) {
    while (inb(KB_STATUS) & 0x01) (void)inb(KB_DATA);
    head = tail = 0;
    shift_down = ctrl_down = alt_down = 0;
}

void kb_handler(void) {
    uint8_t sc = inb(KB_DATA);

    /* release */
    if (sc & 0x80) {
        uint8_t code = sc & 0x7F;
        if (code == 0x2A || code == 0x36) shift_down = 0;
        if (code == 0x1D)                 ctrl_down  = 0;
        if (code == 0x38)                 alt_down   = 0;
        return;
    }

    /* modifiers */
    if (sc == 0x2A || sc == 0x36) { shift_down = 1; return; }
    if (sc == 0x1D)               { ctrl_down  = 1; return; }
    if (sc == 0x38)               { alt_down   = 1; return; }

    char c = shift_down ? map_shift[sc] : map[sc];
    if (!c) return;

    /* Ctrl combos — send control characters */
    if (ctrl_down) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 1);   /* Ctrl+A=1 ... Ctrl+Z=26 */
            else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
    }

    /* Ctrl+Alt+Del → reboot signal (0x03 as special) */
    if (ctrl_down && alt_down && sc == 0x53) {
        /* send 0xFF as "reboot request" */
        int next = (head + 1) % BUF_SIZE;
        if (next != tail) { buf[head] = (char)0xFF; head = next; }
        return;
    }

    int next = (head + 1) % BUF_SIZE;
    if (next != tail) { buf[head] = c; head = next; }
}

int  kb_ready(void) { return head != tail; }
char kb_getchar(void) {
    while (head == tail) __asm__ __volatile__("pause");
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
