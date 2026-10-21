#ifndef BERIN_SCHED_H
#define BERIN_SCHED_H

#include <stdint.h>
#include "idt.h"

#define TASK_READY   0
#define TASK_RUNNING 1
#define TASK_ZOMBIE  2

struct task {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t state;
    uint32_t pad;          // Выравнивание до 8 байт
    uint64_t kernel_rsp;   // Указатель на вершину стека ядра
    uint64_t *pml4;        // Адресное пространство
    struct task *next;
};

void sched_init(void);
struct task *task_create(uint64_t entry, uint64_t *pml4, uint64_t user_rsp);
void schedule(void);
void task_exit(int code);
int sys_fork(struct regs *r);

extern struct task *current_task;
extern void switch_to(struct task *prev, struct task *next);
extern void ret_to_user(struct regs *r);

#endif
