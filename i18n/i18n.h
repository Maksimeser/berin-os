#ifndef BERIN_I18N_H
#define BERIN_I18N_H

typedef enum { LANG_EN = 0, LANG_RU = 1 } lang_t;

enum msg_id {
    MSG_OK = 0,
    MSG_WARN,
    MSG_FAIL,
    MSG_INFO,
    MSG_HELP_HEADER,
    MSG_LOCALE_SET,
    MSG_LOCALE_CURRENT,
    MSG_COUNT
};

void        i18n_set(lang_t l);
lang_t      i18n_get(void);
const char *msg(int id);
const char *lang_name(lang_t l);

#endif
