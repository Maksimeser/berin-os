# Berin OS - Makefile (x86_64, Stage 1)
CC      := gcc
AS      := nasm
LD      := ld

CFLAGS  := -m64 -std=gnu11 -ffreestanding -fno-stack-protector \
           -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 \
           -mno-mmx -mno-avx -mno-80387 -mgeneral-regs-only \
           -Wall -Wextra -O2 \
           -nostdlib -nostartfiles -nodefaultlibs \
           -Iarch/x86_64 -Idrivers -Ikernel -Imm -Iinclude -Iuserland
ASFLAGS := -f elf64
LDFLAGS := -m elf_x86_64 -T linker.ld -nostdlib

BUILD   := build
KERNEL  := berin.bin

C_SRCS   := kernel/kernel.c kernel/panic.c kernel/elf.c kernel/kstring.c \
            kernel/blockdev.c kernel/kberfs.c kernel/vfs.c \
            arch/x86_64/gdt.c arch/x86_64/idt.c arch/x86_64/pic.c arch/x86_64/pit.c \
            arch/x86_64/syscall.c \
            drivers/keyboard.c drivers/ata.c \
            mm/pmm.c mm/paging.c mm/heap.c kernel/sched.c
ASM_SRCS := boot/boot.asm arch/x86_64/isr_stubs.asm arch/x86_64/switch.asm

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
	qemu-system-x86_64 -kernel $(KERNEL) -m 128M -serial stdio \
	    -drive file=disk.img,format=raw,if=ide,cache=writethrough \
	    -no-reboot

debug: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -m 128M -serial stdio \
	    -drive file=disk.img,format=raw,if=ide,cache=writethrough \
	    -no-reboot -d int -D qemu.log

clean:
	rm -rf $(BUILD) $(KERNEL)
