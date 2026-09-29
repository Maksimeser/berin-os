CC      := gcc
AS      := nasm
LD      := ld
INCLUDES := -Idrivers -Idrivers/fonts -Ikernel -Iarch/i386 -Imm -Ii18n

CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-stack-protector \
           -fno-pic -fno-pie -Wall -Wextra -O2 \
           -nostdlib -nostartfiles -nodefaultlibs $(INCLUDES)
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD   := build
KERNEL  := berin.bin

C_SRCS   := kernel/kernel.c kernel/berbash.c kernel/panic.c kernel/timer.c kernel/wait.c drivers/serial.c drivers/keyboard.c drivers/rtc.c drivers/vga_font.c i18n/i18n.c i18n/en.c i18n/ru.c mm/pmm.c mm/paging.c mm/heap.c arch/i386/gdt.c arch/i386/idt.c arch/i386/pic.c arch/i386/pit.c
ASM_SRCS := boot/boot.asm arch/i386/isr_stubs.asm

C_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all clean run

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 128M -serial stdio \
	    -no-reboot -d int -D qemu.log

clean:
	rm -rf $(BUILD) $(KERNEL)
