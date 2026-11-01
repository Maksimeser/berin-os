# Berin OS - Main Makefile

.PHONY: all kernel tools userland run clean

# Путь к папке с ядром (соседняя директория)
KERNEL_DIR := ../berin_kernel

all: kernel tools

# Сборка ядра и копирование бинарника в Turload
kernel:
	$(MAKE) -C $(KERNEL_DIR)
	cp $(KERNEL_DIR)/berin.bin turload/build/esp/berin.bin

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

# ==========================================
# Сборка юзерленда
# ==========================================
userland:
	$(MAKE) -C userland/sh clean
	$(MAKE) -C userland/sh

# ==========================================
# Запуск в QEMU
# ==========================================
run: kernel
	$(MAKE) -C turload run

# Очистка всего
clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	rm -f tools/bformat tools/bfsck tools/bls tools/bput tools/brm
	$(MAKE) -C userland/sh clean || true
