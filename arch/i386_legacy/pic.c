/* Berin OS - 8259A PIC remap
 * Master IRQ0-7 -> INT 32-39
 * Slave  IRQ8-15 -> INT 40-47
 */

#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11
#define ICW4_8086 0x01

void pic_remap(void) {
    outb(PIC1_CMD, ICW1_INIT);
    outb(PIC2_CMD, ICW1_INIT);

    outb(PIC1_DATA, 0x20);   /* master offset 32 */
    outb(PIC2_DATA, 0x28);   /* slave  offset 40 */

    outb(PIC1_DATA, 0x04);   /* slave on IRQ2 */
    outb(PIC2_DATA, 0x02);   /* slave identity */

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, 0xFF);   /* mask all */
    outb(PIC2_DATA, 0xFF);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(irq & 7);
    uint8_t  val  = inb(port);
    outb(port, (uint8_t)(val | (1 << bit)));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (uint8_t)(irq & 7);
    uint8_t  val  = inb(port);
    outb(port, (uint8_t)(val & ~(1 << bit)));
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}
