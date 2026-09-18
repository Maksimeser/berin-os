/* Berin OS - kernel panic
 * Marker format: (1panic1)-NNN_name  (see master doc, section 15)
 */

#include "panic.h"

extern void term_write(const char *s);
extern void term_putchar(char c);
extern void term_set_fg(uint8_t fg);

#define C_GREEN  10
#define C_RED    12
#define C_GRAY    7

static void put_hex32(uint32_t v) {
    static const char hx[] = "0123456789ABCDEF";
    char buf[9];
    for (int i = 7; i >= 0; i--) { buf[i] = hx[v & 0xF]; v >>= 4; }
    buf[8] = 0;
    term_write(buf);
}

static void put_hex16(uint16_t v) {
    static const char hx[] = "0123456789ABCDEF";
    char buf[5];
    for (int i = 3; i >= 0; i--) { buf[i] = hx[v & 0xF]; v >>= 4; }
    buf[4] = 0;
    term_write(buf);
}

static void put_u16_3digits(uint16_t v) {
    term_putchar('0' + (v / 100) % 10);
    term_putchar('0' + (v /  10) % 10);
    term_putchar('0' +  v        % 10);
}

static void halt_forever(void) {
    for (;;) __asm__ __volatile__("cli; hlt");
}

void berin_panic(uint16_t code, const char *name, struct regs *r) {
    __asm__ __volatile__("cli");
    extern void term_clear(void);
    term_clear();

    term_set_fg(C_RED);
    term_write("\n\n");
    term_write("================= BERIN PANIC =================\n\n");

    term_set_fg(C_GRAY);
    term_write("  (1panic1)-");
    put_u16_3digits(code);
    term_putchar('_');
    term_write(name);
    term_write("\n\n");

    term_write("  EAX="); put_hex32(r->eax);
    term_write("  EBX="); put_hex32(r->ebx);
    term_write("  ECX="); put_hex32(r->ecx);
    term_write("  EDX="); put_hex32(r->edx);
    term_putchar('\n');

    term_write("  ESI="); put_hex32(r->esi);
    term_write("  EDI="); put_hex32(r->edi);
    term_write("  EBP="); put_hex32(r->ebp);
    term_write("  ESP="); put_hex32(r->esp_dummy);
    term_putchar('\n');

    term_write("  EIP="); put_hex32(r->eip);
    term_write("  CS=");  put_hex16((uint16_t)r->cs);
    term_write("  DS=");  put_hex16((uint16_t)r->ds);
    term_write("  EFLAGS="); put_hex32(r->eflags);
    term_putchar('\n');

    if (r->err_code) {
        term_write("  ERR="); put_hex32(r->err_code);
        term_putchar('\n');
    }

    term_write("\n  [system halted]\n\n");
    term_write("  Reboot / Перезагрузиться : [Ctrl+Alt+Del]\n");
    term_write("===============================================\n");

    halt_forever();
}
