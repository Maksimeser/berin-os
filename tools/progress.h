#ifndef BERIN_PROGRESS_H
#define BERIN_PROGRESS_H

#include <stdint.h>

/* Multi-line progress bar for host tools.
 *
 *   progress_start("formatting superblock", 1024);
 *   for (i = 0; i < 1024; i++) {
 *       ...
 *       progress_update(i);
 *   }
 *   progress_done(1);   // 1 = OK, 0 = FAIL (prints [ OK ] / [ FAIL ])
 *
 * Behaviour:
 *   - First call prints a new line and remembers it.
 *   - progress_update redraws the same line via \r.
 *   - progress_done finalizes the line (always 100%), moves cursor down.
 *   - A second progress_start after done begins a new line.
 */

void progress_start(const char *label, uint32_t total);
void progress_update(uint32_t current);
void progress_done(int ok);

/* Same, but for FAIL: adds a reason after the bar */
void progress_fail(const char *reason);

#endif
