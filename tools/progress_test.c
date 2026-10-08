#include <stdio.h>
#include <unistd.h>
#include "progress.h"

int main(void) {
    progress_start("step 1: fast", 10);
    for (int i = 0; i <= 10; i++) { progress_update(i); usleep(100000); }
    progress_done(1);

    progress_start("step 2: slower", 50);
    for (int i = 0; i <= 50; i++) { progress_update(i); usleep(50000); }
    progress_done(1);

    progress_start("step 3: fails midway", 100);
    for (int i = 0; i <= 40; i++) { progress_update(i); usleep(30000); }
    progress_fail("not_found: something");

    return 0;
}
