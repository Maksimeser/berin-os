# Berin OS - Universal berbash Makefile

CC      := gcc

# Флаги для Berin OS (x86_64, freestanding, no libc)
BERIN_CFLAGS  := -m64 -std=gnu11 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs \
                 -fno-pic -fno-pie -no-pie -fno-stack-protector -mno-red-zone \
                 -mno-sse -mno-sse2 -mno-mmx -mno-avx -O2 -D__BERIN__ \
                 -I.

# Флаги для BeronyOS (Linux, glibc)
BERONY_CFLAGS := -m64 -std=gnu11 -O2 -Wall -Wextra

all: berin/berbash.elf berin/login.elf berony/berbash

berin/login.elf: login.c libb.c libb.h user.ld
	@mkdir -p berin
	gcc -m64 -std=gnu11 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs \
	    -fno-pic -fno-pie -no-pie -fno-stack-protector -mno-red-zone -mno-sse -mno-sse2 \
	    -mno-mmx -mno-avx -O2 -D__BERIN__ -I. -Wl,-T,user.ld -o $@ login.c libb.c

berin/berbash.elf: berbash.c libb.c libb.h
	@mkdir -p berin
	$(CC) $(BERIN_CFLAGS) -Wl,-T,user.ld -o berin/berbash.elf berbash.c libb.c

berony/berbash: berbash.c
	@mkdir -p berony
	$(CC) $(BERONY_CFLAGS) -o berony/berbash berbash.c

clean:
	rm -f berin/berbash.elf berony/berbash

.PHONY: all clean
