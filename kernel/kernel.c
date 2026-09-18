/* BerinOS - kernel */
#include "pic.h"
#include "pit.h"
#include "gdt.h"
#include "idt.h"
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
static uint8_t term_color;

void term_set_fg(uint8_t fg) {
    term_color = vga_entry_color(fg, VGA_BLACK);
}

void term_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = 0;
    term_col = 0;
}

void term_putchar(char c) {
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
    if (term_row >= VGA_HEIGHT) term_row = 0;

    if (c == '\n') serial_write_char('\r');
    serial_write_char(c);
}

void term_write(const char *s) {
    while (*s) term_putchar(*s++);
}

extern void berbash_run(void);

void kernel_main(uint32_t magic, uint32_t mb_info) {
    (void)mb_info;
        gdt_init();
            idt_init();
            pic_remap();
            pit_init(100);
            pic_unmask(0);
            __asm__ __volatile__("sti");

    serial_init();

    term_color = vga_entry_color(VGA_LIGHT_GREEN, VGA_BLACK);
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
    term_write("Berin v0.0.2\n");
    term_write("\n");

    term_write("\n");

    berbash_run();

    for (;;)
        __asm__ __volatile__("hlt");
}
