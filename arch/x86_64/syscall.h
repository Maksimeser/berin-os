#ifndef BERIN_X86_64_SYSCALL_H
#define BERIN_X86_64_SYSCALL_H

#include "idt.h"

void syscall_dispatch(struct regs *r);

#endif
