#include "i18n.h"

/* NOTE: RU strings are UTF-8. They render fine on serial,
 * but appear garbled on VGA text mode until Phase 7 (framebuffer). */

const char *i18n_ru[MSG_COUNT] = {
    [MSG_OK]             = "[ ОК ]",
    [MSG_WARN]           = "[ ВНИМ ]",
    [MSG_FAIL]           = "[ ОШИБ ]",
    [MSG_INFO]           = "[ ИНФО ]",
    [MSG_HELP_HEADER]    = "kerbash 0.1 — встроенные:",
    [MSG_LOCALE_SET]     = "локаль переключена на",
    [MSG_LOCALE_CURRENT] = "текущая локаль:",
};
