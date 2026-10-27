#include "sched.h"
#include "heap.h"
#include "pmm.h"
#include "kstring.h"
#include "vmm.h"
#include "paging.h"
#include "gdt.h"

#define KSTACK_PAGES 8   /* 32 KB */

/* PID 0 (idle) — статически в .bss, никто не затрёт */
static struct task idle_task;
static uint8_t     idle_kstack[KSTACK_PAGES * 4096] __attribute__((aligned(16)));

struct task *current_task = 0;
static uint32_t next_pid = 1;

extern void term_write(const char *s);
extern void term_putchar(char c);

static void put_hex(uint64_t v) {
    for (int i = 60; i >= 0; i -= 4)
        term_putchar("0123456789ABCDEF"[(v >> i) & 0xF]);
}
static void put_dec(uint32_t v) {
    char b[12]; int n = 0;
    if (!v) { term_putchar('0'); return; }
    while (v) { b[n++] = '0' + (v % 10); v /= 10; }
    while (n) term_putchar(b[--n]);
}

static uint8_t *alloc_kstack(void) {
    uint8_t *base = (uint8_t *)pmm_alloc_page();
    if (!base) return 0;
    for (int i = 1; i < KSTACK_PAGES; i++) {
        void *next = pmm_alloc_page();
        if ((uint64_t)next != (uint64_t)base + (uint64_t)i * 4096) {
            term_write("[ sched ] kstack NOT contiguous\n");
            for (;;) __asm__ __volatile__("cli; hlt");
        }
    }
    return base;
}

void sched_init(void) {
    /* PID 0 — idle, статический */
    memset(&idle_task, 0, sizeof(idle_task));
    idle_task.pid = 0;
    idle_task.state = TASK_IDLE;
    idle_task.next = &idle_task;

    extern uint64_t paging_get_cr3(void);
    idle_task.pml4 = (uint64_t *)paging_get_cr3();
    idle_task.kstack_base = idle_kstack;
    idle_task.kstack_top  = (uint64_t)idle_kstack + sizeof(idle_kstack);

    current_task = &idle_task;
    tss_set_rsp0(idle_task.kstack_top);
    next_pid = 1;

    term_write("[ sched ] init: idle=");       put_hex((uint64_t)&idle_task);
    term_write(" kstack_top=");                put_hex(idle_task.kstack_top);
    term_write(" pml4=");                      put_hex((uint64_t)idle_task.pml4);
    term_putchar('\n');
}

struct task *task_create(uint64_t entry, uint64_t *pml4, uint64_t user_rsp) {
    struct task *t = (struct task *)kmalloc(sizeof(struct task));
    if (!t) { term_write("[ sched ] create: kmalloc failed\n"); return 0; }
    memset(t, 0, sizeof(struct task));
    t->pid = next_pid++;
    t->parent_pid = current_task->pid;
    t->state = TASK_READY;
    t->pml4 = pml4;

    uint8_t *kstack = alloc_kstack();
    if (!kstack) { term_write("[ sched ] create: kstack failed\n"); return 0; }
    t->kstack_base = kstack;
    t->kstack_top  = (uint64_t)kstack + KSTACK_PAGES * 4096;

    uint64_t *sp = (uint64_t *)t->kstack_top;
    *--sp = 0x23;
    *--sp = user_rsp;
    *--sp = 0x202;
    *--sp = 0x1B;
    *--sp = entry;
    *--sp = (uint64_t)task_ret_to_user;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    t->kernel_rsp = (uint64_t)sp;

    t->next = current_task->next;
    current_task->next = t;

    term_write("[ sched ] create pid="); put_dec(t->pid);
    term_write(" t=");                   put_hex((uint64_t)t);
    term_write(" kstack=");              put_hex((uint64_t)kstack);
    term_write(" top=");                 put_hex(t->kstack_top);
    term_write(" pml4=");                put_hex((uint64_t)pml4);
    term_putchar('\n');

    return t;
}

void schedule(void) {
    struct task *prev = current_task;
    struct task *next = current_task->next;
    if (!next || next == current_task) return;

    while (next != current_task) {
        if (next->state == TASK_READY) break;
        next = next->next;
    }
    if (next == current_task) {
        if (prev->state == TASK_ZOMBIE) {
            term_write("[ sched ] no runnable, halt\n");
            for (;;) __asm__ __volatile__("cli; hlt");
        }
        return;
    }

    term_write("[ sched ] prev=");   put_dec(prev->pid);
    term_write(" next=");            put_dec(next->pid);
    term_write(" next->pml4=");      put_hex((uint64_t)next->pml4);
    term_putchar('\n');

    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    current_task = next;
    current_task->state = TASK_RUNNING;
    if (current_task->kstack_top) tss_set_rsp0(current_task->kstack_top);
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(current_task->pml4));
    switch_to(prev, current_task);
}

void task_exit(int code) {
    (void)code;
    current_task->state = TASK_ZOMBIE;
    schedule();
    for (;;) __asm__ __volatile__("cli; hlt");
}
