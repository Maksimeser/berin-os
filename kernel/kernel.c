/* BerinOS - kernel */
#include "heap.h"
#include "version.h"
#include "keyboard.h"
#include "pic.h"
#include "pit.h"
#include "gdt.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include <stdint.h>
#include <stddef.h>
#include "serial.h"


#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

enum vga_color {
    VGA_BLACK       = 0,
    VGA_LIGHT_GREEN = 10,
    VGA_WHITE       = 15,
};

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static size_t  term_row;
static size_t  term_col;
static uint8_t term_fg    = 7;
static uint8_t term_bg    = 0;
static uint8_t term_color = 0x07;

static inline void term_update_color(void) {
    term_color = (uint8_t)((term_bg << 4) | (term_fg & 0x0F));
}

void term_set_fg(uint8_t fg) { term_fg = fg & 0x0F; term_update_color(); }
void term_set_bg(uint8_t bg) { term_bg = bg & 0x0F; term_update_color(); }
void term_reset_color(void)  { term_fg = 7; term_bg = 0; term_update_color(); }

void term_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = 0;
    term_col = 0;
}

static void term_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = VGA_HEIGHT - 1;
}

/* low-level: write ONE character to VGA + serial, no ANSI */
static void term_emit(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\b') {
        if (term_col > 0) term_col--;
    } else {
        if (term_row < VGA_HEIGHT && term_col < VGA_WIDTH)
            VGA_MEMORY[term_row * VGA_WIDTH + term_col] =
            vga_entry((unsigned char)c, term_color);
        term_col++;
    }
    if (term_col >= VGA_WIDTH) { term_col = 0; term_row++; }
    if (term_row >= VGA_HEIGHT) term_scroll();

    if (c == '\n') serial_write_char('\r');
    serial_write_char(c);
}

/* ---------- ANSI escape parser ---------- */
static int ansi_state = 0;      /* 0=normal, 1=ESC, 2=CSI */
static int ansi_param = 0;

static void ansi_sgr(int p) {
    if (p == 0)                    { term_reset_color(); return; }
    if (p == 1)                    { return; }   /* bold — ignore for now */
        if (p >= 30 && p <= 37)        { term_set_fg((uint8_t)(p - 30));     return; }
        if (p >= 40 && p <= 47)        { term_set_bg((uint8_t)(p - 40));     return; }
        if (p >= 90 && p <= 97)        { term_set_fg((uint8_t)(p - 90 + 8)); return; }
        if (p >= 100 && p <= 107)      { term_set_bg((uint8_t)(p - 100 + 8));return; }
}

void term_putchar(char c) {
    if (ansi_state == 2) {                            /* in CSI */
        if (c >= '0' && c <= '9') { ansi_param = ansi_param * 10 + (c - '0'); return; }
        if (c == ';')             { ansi_param = 0; return; }
        ansi_state = 0;
        if (c == 'm')                  ansi_sgr(ansi_param);
        else if (c == 'J' && ansi_param == 2) term_clear();
        else if (c == 'H')             { term_row = 0; term_col = 0; }
        return;
    }
    if (ansi_state == 1) {                            /* after ESC */
        if (c == '[') { ansi_state = 2; ansi_param = 0; return; }
        ansi_state = 0;
        return;                                       /* drop stray ESC */
    }
    if (c == '\033') { ansi_state = 1; return; }

    term_emit(c);
}

void term_write(const char *s) {
    while (*s) term_putchar(*s++);
}

extern void berbash_run(void);

void kernel_main(uint32_t magic, uint32_t mb_info) {
    (void)magic;
    (void)mb_info;
        gdt_init();
            idt_init();
                pmm_init(mb_info);
                paging_init(mb_info);
                    heap_init();
            pic_remap();
            pit_init(100);
            kb_init();
            pic_unmask(0);
            pic_unmask(1);
            __asm__ __volatile__("sti");

    serial_init();

    term_set_fg(VGA_LIGHT_GREEN);
    term_set_bg(VGA_BLACK);
    term_clear();

    term_write("Berin loaded.\n");
    term_write("The system is loading...\n");
    term_write("\n");
    term_write(" /\\     /\\\n");
    term_write("/  \\___/  \\\n");
    term_write("(  o   o  )\n");
    term_write(" \\   \x1F   /\n");
    term_write("  \\_____/\n");
    term_write("\n");
    term_write("Berin v0.0.1\n");
    term_write("\n");

    term_write("\n");

    berbash_run();

    for (;;)
        __asm__ __volatile__("hlt");
}
