#include "i18n.h"

extern const char *i18n_en[MSG_COUNT];
extern const char *i18n_ru[MSG_COUNT];

static lang_t current = LANG_EN;

void i18n_set(lang_t l) { current = l; }
lang_t i18n_get(void)   { return current; }

const char *msg(int id) {
    if (id < 0 || id >= MSG_COUNT) return "?";
    const char **t = (current == LANG_RU) ? i18n_ru : i18n_en;
    const char *s = t[id];
    if (!s && current == LANG_RU) s = i18n_en[id];   /* fallback ru->en */
        return s ? s : "?";
}

const char *lang_name(lang_t l) {
    return (l == LANG_RU) ? "ru" : "en";
}
