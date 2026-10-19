# Berin OS

**Berin. Your own system.**

Hobby OS from scratch. x86_64, long mode, custom kernel, custom filesystem (BerFS), custom shell (kerbash / berbash).

> Mascot: green fox.

## Status

Phase 6.3 — VFS + userland shell (in progress).

Working: boot (PVH + long mode), GDT+TSS, IDT (256 vectors), PIC, PIT (100 Hz), PS/2 keyboard, serial COM1, PMM (bitmap, 4K pages), paging (2 MB huge pages), kmalloc/kfree, ring 3 + int 0x80 syscalls, ELF64 loader, ATA PIO, BerFS v1, VFS, kerbash, berbash (WIP), libb (WIP).

## Build

Requirements: `nasm`, `gcc`, `make`, `qemu-system-x86_64`.

```bash
make clean && make -k -j$(nproc) && make run
