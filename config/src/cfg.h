// cfg: leer/escribir config.ini segun docs\config-spec.md. Tambien fc_log (stderr) para --dry-run.
#ifndef CFG_H
#define CFG_H
#include <wchar.h>

enum { FC_SEED_MAX = 60, FC_SEED_BUF = 256 };   // 60 caracteres UTF-8

typedef struct {
    char seed[FC_SEED_BUF];      // "" = al azar
    double speed;                // 0..400   (20)
    int fps;                     // 5..120   (30)
    double zoom;                 // 0.5..3   (1.0)
    int pause_when_covered;      // 0/1      (1)
    int start_with_windows;      // 0/1      (1)  lo usa la UI para la clave Run
    int variant;                 // fondo elegido: 0 = shan shui (China), 1 = warudopei (Japon)
    unsigned el_off[2];
    int lang;                    // 0 = espanol, 1 = English (idioma de la ventana)          // elementos apagados de cada fondo (bit = indice en FC_ELEMS)
} FcConfig;

typedef struct { const char *key, *label; } FcElem;   // mismo orden que src\elements.h de cada fondo
const FcElem *fc_elems(int variant, int *n);
void fc_cfg_defaults(FcConfig *c);
void fc_cfg_clamp(FcConfig *c);
void fc_cfg_set_seed(FcConfig *c, const char *utf8);       // recorta a 60 caracteres sin partir uno
int  fc_cfg_equal(const FcConfig *a, const FcConfig *b);   // para el estado "sucio"
int  fc_cfg_default_path(wchar_t *out, int n);             // %APPDATA%\FondoShanShui\config.ini
// Superpone el archivo sobre *c (llamar antes a fc_cfg_defaults). 1 = existia y se leyo, 0 = no existe.
int  fc_cfg_load(FcConfig *c, const wchar_t *path);
// Escribe todas las claves (UTF-8 sin BOM, '.' decimal), crea la carpeta si falta y reemplaza
// el archivo de forma atomica. dry_run: solo describe por stderr. 1 = ok.
int  fc_cfg_save(const FcConfig *c, const wchar_t *path, int dry_run);
// Texto del archivo (lo que fc_cfg_save escribiria). Devuelve la longitud.
int  fc_cfg_format(const FcConfig *c, char *out, int outsz);

void fc_log(const char *fmt, ...);   // a stderr (o a la consola padre si es una app de ventana)

#endif
