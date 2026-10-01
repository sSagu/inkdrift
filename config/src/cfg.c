// cfg: config.ini (ver cfg.h y docs\config-spec.md).
#include "cfg.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

void fc_log(const char *fmt, ...) {
    static HANDLE h = NULL;
    if (!h) {
        h = GetStdHandle(STD_ERROR_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) {
            if (AttachConsole(ATTACH_PARENT_PROCESS))
                h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
    }
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof buf - 3) n = sizeof buf - 3;
    buf[n++] = '\r'; buf[n++] = '\n';
    DWORD wr;
    if (h && h != INVALID_HANDLE_VALUE) WriteFile(h, buf, (DWORD)n, &wr, NULL);
    buf[n] = 0;
    OutputDebugStringA(buf);
}

static const FcElem EL_CN[] = {{"trees", "ÁRBOLES"}, {"rocks", "ROCAS"}, {"buildings", "CASAS Y PAGODAS"}, {"towers", "TORRES"},
    {"boats", "BARCAS"}, {"water", "AGUA"}, {"distant", "MONTES LEJANOS"}, {"flat", "LLANURAS"}};
static const FcElem EL_JP[] = {{"trees", "PINOS Y ÁRBOLES"}, {"rocks", "ROCAS"}, {"buildings", "PAGODAS Y CASAS"},
    {"shrines", "TORII Y TUMBAS"}, {"boats", "BARCAS"}, {"water", "AGUA"}, {"distant", "MONTES LEJANOS"}, {"fuji", "FUJI"},
    {"flat", "LLANURAS"}, {"koi", "KOI"}, {"bridges", "PUENTES"}, {"samurai", "SAMURÁIS"}, {"villages", "ALDEAS"},
    {"bamboo", "BAMBÚ"}, {"sakura", "SAKURA"}, {"green", "MONTES VERDES"}, {"easter", "SORPRESAS"}, {"clouds", "NUBES"}, {"waves", "OLAS"}};
static const char *const PREFIX[2] = {"cn", "jp"};
const FcElem *fc_elems(int v, int *n) {
    if (v) { *n = (int)(sizeof EL_JP / sizeof EL_JP[0]); return EL_JP; }
    *n = (int)(sizeof EL_CN / sizeof EL_CN[0]); return EL_CN;
}

void fc_cfg_defaults(FcConfig *c) {
    memset(c, 0, sizeof *c);
    c->speed = 20; c->fps = 30; c->zoom = 1.0; c->pause_when_covered = 1; c->start_with_windows = 1;
}

void fc_cfg_clamp(FcConfig *c) {
    if (!(c->speed >= 0)) c->speed = 0;
    if (c->speed > 400) c->speed = 400;
    if (c->fps < 5) c->fps = 5;
    if (c->fps > 120) c->fps = 120;
    if (!(c->zoom >= 0.5)) c->zoom = 0.5;
    if (c->zoom > 3.0) c->zoom = 3.0;
    c->pause_when_covered = !!c->pause_when_covered;
    c->start_with_windows = !!c->start_with_windows;
}

void fc_cfg_set_seed(FcConfig *c, const char *s) {
    size_t i = 0, o = 0;
    int chars = 0;
    while (s[i] && chars < FC_SEED_MAX) {
        size_t len = 1;
        unsigned char b = (unsigned char)s[i];
        if (b >= 0xF0) len = 4; else if (b >= 0xE0) len = 3; else if (b >= 0xC0) len = 2;
        for (size_t k = 1; k < len; k++) if (!s[i + k]) { len = k; break; }
        if (o + len >= FC_SEED_BUF) break;
        memcpy(c->seed + o, s + i, len);
        o += len; i += len; chars++;
    }
    c->seed[o] = 0;
}

int fc_cfg_equal(const FcConfig *a, const FcConfig *b) {
    return !strcmp(a->seed, b->seed) && fabs(a->speed - b->speed) < 1e-6 && a->fps == b->fps &&
           fabs(a->zoom - b->zoom) < 1e-6 && a->pause_when_covered == b->pause_when_covered &&
           a->start_with_windows == b->start_with_windows && a->variant == b->variant &&
           a->el_off[0] == b->el_off[0] && a->el_off[1] == b->el_off[1] && a->lang == b->lang;
}

int fc_cfg_default_path(wchar_t *out, int n) {
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

int fc_cfg_load(FcConfig *c, const wchar_t *path) {
    if (!path || !path[0]) return 0;
    FILE *f = _wfopen(path, L"rb");
    if (!f) return 0;
    char line[1024];
    int first = 1;
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        if (first && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
        first = 0;
        if (!strchr(line, '\n') && !feof(f)) { int ch; while ((ch = fgetc(f)) != EOF && ch != '\n') {} }
        s = trim(s);
        if (!*s || *s == ';' || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(s), *val = trim(eq + 1);
        double d;
        if (!_stricmp(key, "seed")) fc_cfg_set_seed(c, val);
        else if (!_stricmp(key, "speed")) { if (parse_num(val, &d)) c->speed = d; }
        else if (!_stricmp(key, "fps")) { if (parse_num(val, &d)) c->fps = d > 1e6 ? 1000000 : d < -1e6 ? -1000000 : (int)floor(d + 0.5); }
        else if (!_stricmp(key, "zoom")) { if (parse_num(val, &d)) c->zoom = d; }
        else if (!_stricmp(key, "pause_when_covered")) { if (parse_num(val, &d)) c->pause_when_covered = d != 0; }
        else if (!_stricmp(key, "start_with_windows")) { if (parse_num(val, &d)) c->start_with_windows = d != 0; }
        else if (!_stricmp(key, "fondo")) c->variant = !_stricmp(val, "nippon");
        else if (!_stricmp(key, "lang")) c->lang = !_stricmp(val, "en");
        else {
            for (int v = 0; v < 2; v++) {
                size_t pl = strlen(PREFIX[v]);
                if (_strnicmp(key, PREFIX[v], pl) || key[pl] != '.') continue;
                int n; const FcElem *E = fc_elems(v, &n);
                for (int i = 0; i < n; i++)
                    if (!_stricmp(key + pl + 1, E[i].key) && parse_num(val, &d)) {
                        if (d != 0) c->el_off[v] &= ~(1u << i); else c->el_off[v] |= 1u << i;
                    }
            }
        }
    }
    fclose(f);
    fc_cfg_clamp(c);
    return 1;
}

// numero con '.' decimal y sin ceros sobrantes (independiente del locale: se arma a mano)
static void fmt_num(double v, char *out) {
    long long m = (long long)floor(v * 1000 + 0.5);
    long long ip = m / 1000, fp = m % 1000;
    if (fp == 0) { sprintf(out, "%lld", ip); return; }
    char f[8];
    sprintf(f, "%03lld", fp);
    for (int i = 2; i > 0 && f[i] == '0'; i--) f[i] = 0;
    sprintf(out, "%lld.%s", ip, f);
}

int fc_cfg_format(const FcConfig *in, char *out, int outsz) {
    FcConfig c = *in;
    fc_cfg_clamp(&c);
    char sp[32], zo[32];
    fmt_num(c.speed, sp);
    fmt_num(c.zoom, zo);
    int len = snprintf(out, (size_t)outsz,
        "; Fondo shan-shui / nippon: configuracion (la escribe fondo-config.exe)\n"
        "lang=%s\nfondo=%s\nseed=%s\nspeed=%s\nfps=%d\nzoom=%s\npause_when_covered=%d\nstart_with_windows=%d\n",
        c.lang ? "en" : "es", c.variant ? "nippon" : "shanshui", c.seed, sp, c.fps, zo, c.pause_when_covered, c.start_with_windows);
    for (int v = 0; v < 2 && len > 0 && len < outsz; v++) {
        int n; const FcElem *E = fc_elems(v, &n);
        for (int i = 0; i < n && len < outsz; i++)
            len += snprintf(out + len, (size_t)(outsz - len), "%s.%s=%d\n", PREFIX[v], E[i].key, !(c.el_off[v] & (1u << i)));
    }
    return len;
}

// crea todas las carpetas de la ruta (sin el nombre de archivo)
static int make_dirs(const wchar_t *path) {
    wchar_t d[MAX_PATH];
    wcsncpy(d, path, MAX_PATH - 1);
    d[MAX_PATH - 1] = 0;
    wchar_t *slash = wcsrchr(d, L'\\');
    if (!slash) return 1;
    *slash = 0;
    for (wchar_t *p = d + 3; *p; p++) {
        if (*p == L'\\') { *p = 0; CreateDirectoryW(d, NULL); *p = L'\\'; }
    }
    return CreateDirectoryW(d, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

int fc_cfg_save(const FcConfig *c, const wchar_t *path, int dry) {
    char txt[1024];
    int n = fc_cfg_format(c, txt, sizeof txt);
    if (n <= 0 || n >= (int)sizeof txt) return 0;
    if (dry) {
        char p8[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, p8, sizeof p8, NULL, NULL);
        fc_log("[dry-run] escribiria %s:\n%s", p8, txt);
        return 1;
    }
    if (!make_dirs(path)) return 0;
    wchar_t tmp[MAX_PATH + 8];
    _snwprintf(tmp, MAX_PATH + 8, L"%ls.tmp", path);
    tmp[MAX_PATH + 7] = 0;
    HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD wr = 0;
    BOOL ok = WriteFile(h, txt, (DWORD)n, &wr, NULL) && wr == (DWORD)n;
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(tmp); return 0; }
    return 1;
}
