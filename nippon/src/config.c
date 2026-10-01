// Lectura de config.ini (contrato en docs\config-spec.md).
//   - UTF-8 (con o sin BOM), una opcion por linea "clave=valor".
//   - Lineas que empiezan con ';' o '#' son comentarios. Se ignoran espacios alrededor,
//     lineas vacias y claves desconocidas. En los valores numericos tambien se acepta un
//     comentario al final ("speed = 30 ; rapido") y la coma decimal ("1,5").
//   - Valores fuera de rango se recortan; valores ilegibles dejan el valor anterior (el por
//     defecto, si se parte de config_defaults).
// Solo usa la biblioteca de C: este archivo tambien entra en el banco de pruebas de check.ps1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>
#include "config.h"
#include "elements.h"

void config_defaults(Config *c) {
    memset(c, 0, sizeof *c);
    c->speed = 20;
    c->fps = 30;
    c->zoom = 1.0;
    c->pause_when_covered = 1;
}

void config_clamp(Config *c) {
    if (!(c->speed >= 0)) c->speed = 0;          // tambien NaN
    if (c->speed > 400) c->speed = 400;
    if (c->fps < 5) c->fps = 5;
    if (c->fps > 120) c->fps = 120;
    if (!(c->zoom >= 0.5)) c->zoom = 0.5;
    if (c->zoom > 3.0) c->zoom = 3.0;
    c->pause_when_covered = c->pause_when_covered ? 1 : 0;
}

void config_set_seed(Config *c, const char *s) {
    size_t i = 0, o = 0;
    int chars = 0;
    while (s[i] && chars < CFG_SEED_MAX) {
        size_t len = 1;
        unsigned char b = (unsigned char)s[i];
        if (b >= 0xF0) len = 4; else if (b >= 0xE0) len = 3; else if (b >= 0xC0) len = 2;
        for (size_t k = 1; k < len; k++) if (!s[i + k]) { len = k; break; }
        if (o + len >= CFG_SEED_BUF) break;
        memcpy(c->seed + o, s + i, len);
        o += len;
        i += len;
        chars++;
    }
    c->seed[o] = 0;
}

int config_default_path(wchar_t *out, int n) {
    const wchar_t *ad = _wgetenv(L"APPDATA");
    if (!ad || !ad[0]) { if (n > 0) out[0] = 0; return 0; }
    int r = _snwprintf(out, (size_t)n, L"%ls\\FondoShanShui\\config.ini", ad);
    if (r < 0 || r >= n) { out[0] = 0; return 0; }
    return 1;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

// Numero con '.' o ',' decimal, admite un comentario al final. 0 si es ilegible.
static int parse_num(const char *v, double *out) {
    char buf[64];
    size_t n = strlen(v);
    if (n == 0 || n >= sizeof buf) return 0;
    memcpy(buf, v, n + 1);
    for (char *p = buf; *p; p++) if (*p == ',') *p = '.';
    char *end;
    double d = strtod(buf, &end);
    if (end == buf || !isfinite(d)) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end && *end != ';' && *end != '#') return 0;
    *out = d;
    return 1;
}

int config_load(Config *c, const wchar_t *path) {
    if (!path || !path[0]) return 0;
    FILE *f = _wfopen(path, L"rb");
    if (!f) return 0;
    char line[1024];
    int first = 1;
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        if (first && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
        first = 0;
        // linea demasiado larga: descartar el resto
        if (!strchr(line, '\n') && !feof(f)) { int ch; while ((ch = fgetc(f)) != EOF && ch != '\n') {} }
        s = trim(s);
        if (!*s || *s == ';' || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(s), *val = trim(eq + 1);
        double d;
        if (!_stricmp(key, "seed")) {
            config_set_seed(c, val);
        } else if (!_stricmp(key, "speed")) {
            if (parse_num(val, &d)) c->speed = d;
        } else if (!_stricmp(key, "fps")) {
            if (parse_num(val, &d)) c->fps = d > 1e6 ? 1000000 : d < -1e6 ? -1000000 : (int)floor(d + 0.5);
        } else if (!_stricmp(key, "zoom")) {
            if (parse_num(val, &d)) c->zoom = d;
        } else if (!_stricmp(key, "pause_when_covered")) {
            if (parse_num(val, &d)) c->pause_when_covered = d != 0;
        }
        else {
            // "<prefijo>.<elemento>=0/1" (cada fondo lee solo los suyos)
            size_t pl = strlen(EL_PREFIX);
            if (!_strnicmp(key, EL_PREFIX, pl) && key[pl] == '.') {
                for (int i = 0; i < EL_COUNT; i++)
                    if (!_stricmp(key + pl + 1, EL_NAMES[i]) && parse_num(val, &d)) {
                        if (d != 0) c->el_off &= ~(1u << i); else c->el_off |= 1u << i;
                    }
            }
        }
        // start_with_windows, fondo y claves desconocidas: se ignoran
    }
    fclose(f);
    config_clamp(c);
    return 1;
}
