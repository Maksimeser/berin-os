#include "progress.h"
#include <stdio.h>
#include <string.h>

#define BAR_WIDTH 24

static char     g_label[64];
static int      g_label_len;
static uint32_t g_total;
static uint32_t g_current;
static int      g_active;

static void redraw(void) {
    /* [##########----------] 100% */
    uint32_t pct = g_total ? (g_current * 100u) / g_total : 0;
    if (pct > 100) pct = 100;
    int filled = (int)((pct * BAR_WIDTH) / 100);

    printf("\r%-32s [", g_label);
    for (int i = 0; i < BAR_WIDTH; i++) putchar(i < filled ? '#' : '-');
    printf("] %3u%%", pct);
    fflush(stdout);
}

void progress_start(const char *label, uint32_t total) {
    int n = 0;
    while (label[n] && n < (int)sizeof(g_label) - 1) {
        g_label[n] = label[n];
        n++;
    }
    g_label[n] = '\0';
    g_label_len = n;
    g_total     = total ? total : 1;
    g_current   = 0;
    g_active    = 1;
    redraw();
}

void progress_update(uint32_t current) {
    if (!g_active) return;
    g_current = current;
    if (g_current > g_total) g_current = g_total;
    redraw();
}

static void finish_line(const char *status, const char *reason) {
    if (!g_active) return;
    g_current = g_total;
    redraw();
    if (reason) {
        printf("  %s %s", status, reason);
    } else {
        printf("  %s", status);
    }
    putchar('\n');
    fflush(stdout);
    g_active = 0;
}

void progress_done(int ok) {
    finish_line(ok ? "[ OK ]" : "[ FAIL ]", NULL);
}

void progress_fail(const char *reason) {
    finish_line("[ FAIL ]", reason);
}
