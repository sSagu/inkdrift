// Idioma de la interfaz: la fuente traduce cada texto antes de dibujarlo o medirlo.
#ifndef I18N_H
#define I18N_H
extern int g_lang_en;                 // 0 = espanol, 1 = English
const char *i18n_tr(const char *s);   // devuelve la traduccion (o s si no hay)
#endif
