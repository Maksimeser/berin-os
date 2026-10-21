#include "sched.h"
#include "heap.h"
#include "pmm.h"
#include "kstring.h"
#include "vmm.h"

struct task *current_task = 0;
static struct task *task_list = 0;
static uint32_t next_pid = 1;

extern void task_ret_to_user(void);
extern void exit_to_kernel(uint64_t code);

void sched_init(void) {
    current_task = (struct task *)kmalloc(sizeof(struct task));
    memset(current_task, 0, sizeof(struct task));
    current_task->pid = 0;
    current_task->state = TASK_RUNNING;
    current_task->next = current_task;
    task_list = current_task;
}

struct task *task_create(uint64_t entry, uint64_t *pml4, uint64_t user_rsp) {
    struct task *t = (struct task *)kmalloc(sizeof(struct task));
    memset(t, 0, sizeof(struct task));
    t->pid = next_pid++;
    t->parent_pid = current_task->pid;
    t->state = TASK_READY;
    t->pml4 = pml4;

    // Выделяем ядерный стек (16 КБ)
    void *kstack = pmm_alloc_page();
    if (!kstack) return 0;

    uint64_t *sp = (uint64_t *)((uint64_t)kstack + 4096);

    // 1. Фрейм для iretq (возврат в userland)
    *--sp = 0x23;         // SS (user data)
    *--sp = user_rsp;     // RSP (user)
    *--sp = 0x202;        // RFLAGS (IF=1)
    *--sp = 0x1B;         // CS (user code)
    *--sp = entry;        // RIP (куда прыгнуть в userland)

    // 2. Возврат для switch_to (он сделает ret и прыгнет в task_ret_to_user)
    *--sp = (uint64_t)task_ret_to_user;

    // 3. Фейковый контекст для switch_to
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15

    t->kernel_rsp = (uint64_t)sp;

    // Добавляем в кольцо
    t->next = task_list->next;
    task_list->next = t;
    return t;
}

void schedule(void) {
    struct task *prev = current_task;
    struct task *next = current_task->next;

    // Ищем готовую задачу
    while (next != current_task) {
        if (next->state == TASK_READY) break;
        next = next->next;
    }

    // Если нет готовых задач, никуда не переключаемся
    if (next == current_task) return;

    current_task = next;
    current_task->state = TASK_RUNNING;

    // Переключаем адресное пространство
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(current_task->pml4));

    switch_to(prev, current_task);
}

void task_exit(int code) {
    current_task->state = TASK_ZOMBIE;

    // Ищем, кому передать управление
    struct task *next = task_list;
    int found = 0;
    while (next) {
        if (next->state == TASK_READY) { found = 1; break; }
        next = next->next;
        if (next == task_list) break;
    }

    if (found) {
        schedule(); // Уходим в другую задачу
    } else {
        // Нет задач, возвращаемся в kerbash
        exit_to_kernel((uint64_t)code);
    }
}
