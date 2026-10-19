#ifndef BERIN_SCHED_H
#define BERIN_SCHED_H

#include <stdint.h>
#include "idt.h"

struct task {
    int pid;
    int parent_pid;
    int state;          /* 0 = READY, 1 = RUNNING, 2 = ZOMBIE */
    struct regs regs;   /* Сохраненные регистры userland */
    uint64_t kernel_rsp;
    uint64_t user_rsp;
    struct task *next;
};

void sched_init(void);
int  sys_fork(struct regs *r);
void task_exit(int code);
void schedule(void);

extern struct task *current_task;
extern void switch_to(struct task *prev, struct task *next);

#endif
