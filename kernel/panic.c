/* Berin - kernel panic
 * Marker: (1panic1)-NNN_name
 */
#include "panic.h"
#include "io.h"
#include "term.h"

static void put_hex64(uint64_t v) {
    static const char hx[] = "0123456789ABCDEF";
    char buf[17];
    for (int i = 15; i >= 0; i--) { buf[i] = hx[v & 0xF]; v >>= 4; }
    buf[16] = 0;
    term_write(buf);
}

static void put_u16_3(uint16_t v) {
    term_putchar('0' + (v / 100) % 10);
    term_putchar('0' + (v /  10) % 10);
    term_putchar('0' +  v        % 10);
}

void berin_panic(uint16_t code, const char *name, struct regs *r) {
    __asm__ __volatile__("cli");

    extern void term_clear(void);
    extern void term_reset_color(void);
    term_reset_color();
    term_set_fg(12);
    term_set_bg(0);
    term_clear();
    term_write("================= BERIN PANIC =================\n\n");
    term_write("  (1panic1)-");
    put_u16_3(code);
    term_putchar('_');
    term_write(name);
    term_write("\n\n");

    term_write("  RAX="); put_hex64(r->rax);
    term_write("  RBX="); put_hex64(r->rbx);
    term_write("  RCX="); put_hex64(r->rcx);
    term_putchar('\n');
    term_write("  RDX="); put_hex64(r->rdx);
    term_write("  RSI="); put_hex64(r->rsi);
    term_write("  RDI="); put_hex64(r->rdi);
    term_putchar('\n');
    term_write("  RBP="); put_hex64(r->rbp);
    term_write("  RSP="); put_hex64(r->rsp);
    term_write("  RIP="); put_hex64(r->rip);
    term_putchar('\n');
    term_write("  CS=");  put_hex64(r->cs);
    term_write("  FLG="); put_hex64(r->rflags);
    term_putchar('\n');
    if (r->err_code) {
        term_write("  ERR="); put_hex64(r->err_code);
        term_putchar('\n');
    }

    term_write("\n  [system halted]\n");
    term_write("  Reboot : [Ctrl+Alt+Del]\n");
    term_write("===============================================\n");

    /* Ctrl+Alt+Del polling — we can't use IRQ here (interrupts off).
     * Scancode sequence: Ctrl=0x1D, Alt=0x38, Del=0x53. */
__asm__ __volatile__("cli");
int have_ctrl = 0, have_alt = 0;
    for (;;) {
        if (inb(0x64) & 0x01) {
            uint8_t sc = inb(0x60);
            if (sc == 0x1D) have_ctrl = 1;
            else if (sc == 0x38) have_alt = 1;
            else if (sc == 0x53 && have_ctrl && have_alt) {
                /* hardware reset via 8042 */
                while (inb(0x64) & 0x02) ;
                outb(0x64, 0xFE);
                /* fallback: ACPI reset */
                outw(0x604, 0x2000);
                outb(0xCF9, 0x06);
                /* if nothing worked — hang */
                for (;;) __asm__ __volatile__("cli; hlt");
            }
            /* release codes — reset flags */
            else if (sc == 0x9D) have_ctrl = 0;
            else if (sc == 0xB8) have_alt = 0;
        } else {
            __asm__ __volatile__("pause");
        }
    }
}
