// Lectura de %APPDATA%\FondoShanShui\config.ini (ver docs\config-spec.md).
// Solo usa la biblioteca de C (tambien se compila dentro del banco de pruebas de check.ps1).
#ifndef CONFIG_H
#define CONFIG_H
#include <wchar.h>

enum { CFG_SEED_MAX = 60, CFG_SEED_BUF = 256 };   // 60 caracteres UTF-8 (hasta 4 bytes c/u)

typedef struct {
    char seed[CFG_SEED_BUF];       // "" = al azar en cada arranque
    double speed;                  // unidades del mundo por segundo   [0, 400]
    int fps;                       // cuadros por segundo              [5, 120]
    double zoom;                   //                                  [0.5, 3]
    int pause_when_covered;        // 0/1
    unsigned el_off;               // elementos apagados (bit = indice en elements.h)
} Config;

void config_defaults(Config *c);
void config_clamp(Config *c);
// Ruta por defecto: %APPDATA%\FondoShanShui\config.ini. Devuelve 0 si no hay APPDATA.
int config_default_path(wchar_t *out, int n);
// Superpone lo que haya en el archivo sobre *c. Devuelve 1 si se pudo leer, 0 si no existe.
int config_load(Config *c, const wchar_t *path);
// Copia la semilla recortada a CFG_SEED_MAX caracteres (UTF-8) sin partir un caracter.
void config_set_seed(Config *c, const char *s);

#endif
