#include "sched.h"
#include "pmm.h"
#include "heap.h"
#include "pmm.h"
#include "kstring.h"

struct task *current_task = 0;
static struct task *task_list = 0;
static int next_pid = 1;

void sched_init(void) {
    current_task = (struct task *)kmalloc(sizeof(struct task));
    memset(current_task, 0, sizeof(struct task));
    current_task->pid = 0;
    current_task->state = 1; /* RUNNING */
    current_task->next = current_task;
    task_list = current_task;
}

int sys_fork(struct regs *r) {
    struct task *child = (struct task *)kmalloc(sizeof(struct task));
    memset(child, 0, sizeof(struct task));

    child->pid = next_pid++;
    child->parent_pid = current_task->pid;
    child->state = 0; /* READY */
    child->regs = *r;

    /* Копируем стек пользователя */
    uint64_t new_stack_base = (uint64_t)pmm_alloc_page();
    uint64_t old_stack_base = r->rsp & ~0xFFFULL;
    memcpy((void *)new_stack_base, (void *)old_stack_base, 4096);

    child->regs.rsp = new_stack_base + (r->rsp & 0xFFF);
    child->regs.rax = 0; /* Дочерний процесс получает 0 от fork */

    child->next = task_list;
    task_list = child;

    return child->pid; /* Родитель получает PID ребенка */
}

void schedule(void) {
    struct task *prev = current_task;
    struct task *next = current_task->next;

    while (next) {
        if (next->state == 0) break; /* Нашли READY */
            next = next->next;
    }
    if (!next) return;

    current_task = next;
    current_task->state = 1; /* RUNNING */
    switch_to(prev, current_task);
}

void task_exit(int code) {
    (void)code;
    current_task->state = 2; /* ZOMBIE */

    struct task *next = task_list;
    while (next) {
        if (next->state == 0) break;
        next = next->next;
    }
    if (next) {
        current_task = next;
        current_task->state = 1;
        switch_to(current_task, current_task); /* Возвращаемся в родителя */
    }
}
