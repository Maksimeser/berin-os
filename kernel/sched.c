#include "sched.h"
#include "heap.h"
#include "pmm.h"
#include "kstring.h"
#include "vmm.h"
#include "paging.h"

struct task *current_task = 0;
static struct task *task_list = 0;
static uint32_t next_pid = 1;

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

    void *kstack = pmm_alloc_page();
    if (!kstack) return 0;

    uint64_t *sp = (uint64_t *)((uint64_t)kstack + 4096);

    // iretq frame (Ring 3)
    *--sp = 0x23;         // SS
    *--sp = user_rsp;     // RSP
    *--sp = 0x202;        // RFLAGS
    *--sp = 0x1B;         // CS
    *--sp = entry;        // RIP

    // Возврат из switch_to для новой задачи
    *--sp = (uint64_t)task_ret_to_user;

    // Фейковые регистры для switch_to
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

    // Если нет других задач, остаемся здесь
    if (next == current_task) return;

    // ВАЖНО: Помечаем текущую задачу как готовую к возобновлению!
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }

    current_task = next;
    current_task->state = TASK_RUNNING;

    // Переключаем адресное пространство
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(current_task->pml4));

    switch_to(prev, current_task);
}

void task_exit(int code) {
    (void)code;
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
        schedule();
    } else {
        extern void exit_to_kernel(uint64_t code);
        exit_to_kernel(0); // Выходим в kerbash
    }
}
