#ifndef BERIN_SCHED_H
#define BERIN_SCHED_H

#include <stdint.h>
#include "idt.h"

#define TASK_READY   0
#define TASK_RUNNING 1
#define TASK_ZOMBIE  2
#define TASK_IDLE    3

//#define KSTACK_SIZE  (128 * 1024)

struct task {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t state;
    uint32_t pad;
    uint64_t kernel_rsp;
    uint64_t *pml4;
    struct task *next;
    uint64_t kstack_top;
    void    *kstack_base;
};

void sched_init(void);
struct task *task_create(uint64_t entry, uint64_t *pml4, uint64_t user_rsp);
void schedule(void);
void task_exit(int code);

extern struct task *current_task;
extern void switch_to(struct task *prev, struct task *next);
extern void task_ret_to_user(void);

#endif
