# 🦊 Berin OS

**Своя система. Чужой софт не запрещён.**

Berin OS — это операционная система с нуля (Unix-like, но не копия Linux). 
Этот репозиторий содержит userland, загрузчик (`turload`), хост-утилиты и скрипты сборки.

> Mascot: Sorsha, green fox.
> Slogan: "Бери. Своя система."

## Экосистема Turker Stack

- **Berin OS** — своё ядро (x86_64), свой userland, свой шелл.
- **Berony Linux** — практический дистрибутив на базе LFS 13.1 (Wayland, systemd).
- **BGlade** — графическая среда (Corporate-Square стиль). Сначала для Berony (Linux), потом для Berin.
- **pinit (Piper)** — Главный Домовой, PID 1 в Berin OS. 
- **Skady** — файловый менеджер (TUI/GUI, волчица).
- **Elara** — текстовый редактор (TUI/GUI, кошка).

## Технологический стек

- **Ядро и системный софт:** C + NASM.
- **Приложения и часть BGlade:** C# (логика, плагины, UI-тулинг).
- **Скрипты ядра:** Bash/Make.
- **Никакого C++:** (Ни в ядре, ни в userland).

## Структура проекта

```text
berin/
├── turload/          — UEFI bootloader (свой загрузчик)
├── userland/sh/      — Universal berbash + libb (C)
│   ├── berin/        — сборка под Berin OS (Ring 3)
│   └── berony/       — сборка под Linux
├── tools/            — Хост-утилиты: bformat, bfsck, bls, bput, brm
├── i18n/             — Локализация (en/ru)
├── include/          — Общие заголовки (berfs.h)
├── docs/             — Документация (00-МАСТЕР.md)
├── disk.img          — Образ BerFS (64 MB)
└── Makefile
Сборка и запуск (Arch Linux)
Требования: nasm, gcc, make, qemu-system-x86 (+KVM), mtools.

bash
# 1. Собрать ядро (вызывает make в ~/berin_kernel)
make kernel

# 2. Собрать userland и tools
make tools userland

# 3. Обновить образ диска (disk.img)
make disk

# 4. Запустить в QEMU с KVM
make run
Status (Phase 6.3 CLOSED)
login.elf и berbash.elf запускаются в Ring 3 через SYS_SPAWN.

Работает run /hello.elf (без паники, wait4 корректно реапит процесс).

kdebug on/off включает и выключает отладочные логи ядра.

fork/execve находятся в разработке (Phase 6.4).

Философия
Языки: Ядро — C + NASM. C# — только для аппов и BGlade. Rust — только для отдельных драйверов (Фаза 10+).

Антимонолит: В будущем (Фаза 8+) драйверы выносятся в загружаемые модули (.bmod).

UX: Пользователь всегда прав. Ошибки выдаются все сразу, понятным языком.

Разработано при участии AI-ассистентов: DeepSeek, Claude, GPT.
