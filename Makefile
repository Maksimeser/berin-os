# Berin - Makefile
CC      := gcc
AS      := nasm
LD      := ld

CFLAGS  := -m64 -std=gnu11 -ffreestanding -fno-stack-protector \
	   -fno-pic -fno-pie  -mno-red-zone \
           -mno-mmx -mno-avx -mno-80387 -mgeneral-regs-only \
           -Wall -Wextra -O2 \
           -nostdlib -nostartfiles -nodefaultlibs \
           -Iarch/x86_64 -Idrivers -Ikernel -Imm -Iinclude -Iuserland
ASFLAGS := -f elf64
LDFLAGS := -m elf_x86_64 -T linker.ld -nostdlib

BUILD   := build
KERNEL  := berin.bin

C_SRCS   := kernel/kernel.c kernel/panic.c kernel/elf.c kernel/kstring.c \
            kernel/blockdev.c kernel/kberfs.c kernel/vfs.c kernel/sched.c kernel/fat32.c \
            mm/pmm.c mm/paging.c mm/heap.c mm/vmm.c \
            arch/x86_64/gdt.c arch/x86_64/idt.c arch/x86_64/pic.c arch/x86_64/pit.c arch/x86_64/syscall.c \
            drivers/keyboard.c drivers/ata.c
ASM_SRCS := boot/boot.asm arch/x86_64/isr_stubs.asm arch/x86_64/switch.asm

C_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all clean run debug tools

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
	    -drive file=fat.img,format=raw,if=ide,index=1 \
	    -no-reboot

debug: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -m 128M -serial stdio \
	    -drive file=disk.img,format=raw,if=ide,cache=writethrough \
	    -no-reboot -d int -D qemu.log

clean:
	rm -rf $(BUILD) $(KERNEL) tools/bformat tools/bfsck tools/bls tools/bput tools/brm

# ==========================================
# Хостовые утилиты для BerFS
# ==========================================

TOOL_COMMON := tools/berfs_host.c tools/berfs_dir.c tools/berfs_alloc.c
TOOLS       := bformat bfsck bls bput brm

tools: $(addprefix tools/, $(TOOLS))

tools/bformat: tools/bformat.c $(TOOL_COMMON)
	gcc -O2 -o $@ $< $(TOOL_COMMON)

tools/bfsck: tools/bfsck.c $(TOOL_COMMON)
	gcc -O2 -o $@ $< $(TOOL_COMMON)

tools/bls: tools/bls.c $(TOOL_COMMON)
	gcc -O2 -o $@ $< $(TOOL_COMMON)

tools/bput: tools/bput.c $(TOOL_COMMON)
	gcc -O2 -o $@ $< $(TOOL_COMMON)

tools/brm: tools/brm.c $(TOOL_COMMON)
	gcc -O2 -o $@ $< $(TOOL_COMMON)
