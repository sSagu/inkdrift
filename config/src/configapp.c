// fondo-config.exe: ventana de configuración del fondo animado (estilo Loop Hero, variante B).
// Geometría, paleta, textos y estados: ui\design\project\Spec.dc.html (lienzo lógico 400x260).
// Contrato: docs\config-spec.md. Reglas (sucio, Aplicar, Nuevo paisaje): ui\research\loophero-estructura.md 7.4.
//
// Uso:
//   fondo-config.exe                         ventana normal (lee config.ini y la clave Run)
//   --config <ruta>                          otro config.ini (lectura y escritura reales en esa ruta)
//   --dry-run                                no toca el registro ni manda mensajes al fondo (solo los describe
//                                            por stderr); sin --config tampoco escribe config.ini
//   --fake-fondo 0|1                         fuerza "el fondo corre / no corre" (pruebas)
//   --shot <png> [--scale N] [--state anda|sucio|sinfondo|guardado|error] [--tip]
//                                            renderiza un cuadro y lo guarda, sin ventana ni efectos
//   --quit-after <ms>                        cierra sola (prueba de humo)
//   --selftest --config <ruta temporal>      pruebas sin ventana; registro y mensajes siempre en dry-run
//
// Los widgets del toolkit se usan solo por su lógica (foco, teclado, mouse, hold): se dibujan sobre un
// lienzo descartable y la apariencia exacta del diseño se dibuja acá sobre el lienzo real.
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "px.h"
#include "font.h"
#include "ui.h"
#include "pxwin.h"
#include "cfg.h"
#include "sysmod.h"
#include "i18n.h"

enum { CW = 400, CH = 260 };
enum { ID_SEED = 1, ID_DIE, ID_FORGET, ID_SPEED, ID_FPS, ID_ZOOM, ID_PAUSE, ID_START, ID_RESET, ID_NEW, ID_APPLY,
       ID_RETREAT, ID_VAR0 = 100, ID_EL0 = 110, ID_LANG = 140, ID_CLOSE = 0x7FFF0101 };
enum { Z_RIGHT, Z_BOTTOM, Z_TITLE };   // colocación del tooltip
enum { HOLD_MS = 900, FLASH_MS = 100, SAVED_MS = 3000, POLL_MS = 2000, TAPTIP_MS = 2000 };

typedef struct {
    FcConfig cfg, saved;
    wchar_t path[MAX_PATH];
    int dry_cfg, dry_sys;
    int file_exists;            // config.ini existía (o ya se escribió)
    int run_state;              // FC_RUN_* de la clave Run
    int fake_fondo;             // -1 = FindWindow real
    int running, polled;
    uint32_t last_poll;
    int error;                  // falló el último Aplicar
    uint32_t saved_until, quit_at, taptip_until;
    // tooltip propio (también sobre controles deshabilitados)
    int hov_id, hov_cand, tip_hidden;
    uint32_t hov_since;
    int tip_id, tip_zone;
    PxRect tip_anchor, tip_rect;
    const char *tip_title, *tip_func, *tip_amb;
    // diagnóstico (selftest)
    int n_save, n_reload, n_launch, n_new, n_quit;
    const char *st1, *main_label;
    int main_enabled, retreat_enabled, dirty;
    PxCanvas scratch;
    HWND hwnd;
} App;

static const double SPEED_TICKS[] = {0, 20, 50, 100, 200, 400}, FPS_TICKS[] = {5, 15, 30, 60, 120},
                    ZOOM_TICKS[] = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
static const UiRange R_SPEED = {0, 400, 2, 1, 10, NULL, 0, NULL, NULL, NULL};   // speed = 400·t²
static const UiRange R_FPS = {5, 120, 1, 1, 10, NULL, 0, NULL, NULL, NULL};
static const UiRange R_ZOOM = {0.5, 3.0, 1, 0.1, 0.5, NULL, 0, NULL, NULL, NULL};

// ------------------------------------------------------------------ íconos (Spec §9)
static const char IC_MONTE[] = "...#..... ..#+#.... .#+++#.#. #+++++#+# #########";
static const char IC_X[] = "#...# .#.#. ..#.. .#.#. #...#";
static const char IC_DADO[] = ".#####. #o+++o# #+++++# #++o++# #+++++# #o+++o# .#####.";
static const char IC_OLVIDAR[] = ".+++++. +++++++ +oo+oo+ +oo+oo+ +++o+++ .+++++. .+.+.+.";
static const char IC_FOGATA[] = "...*... ..**... ..*#*.. .*###*. .*#Y#*. ++*#*++ .+++++.";
static const char IC_CORREDOR[] = "......##... ......##... ....####... ..####..#.. .#..##..... ...###..... "
                                  "..##.##.... .##...##... ##.....#... .......##..";
__attribute__((unused)) static const char IC_SELLO[] ="######### ######### ####+#### ##+#+#+## ##+#+#+## ##+++++## ######### #########";

static void bmp(PxCanvas *c, int x, int y, const char *rows, const char *keys, const uint32_t *cols) {
    int i = 0, j = 0;
    for (const char *p = rows; *p; p++) {
        if (*p == ' ') { j++; i = 0; continue; }
        const char *k = strchr(keys, *p);
        if (*p != '.' && k) px_pset(c, x + i, y + j, cols[k - keys]);
        i++;
    }
}

// La F2 del diseño (5 px con astas de 2 px) no existe en el toolkit: título, encabezados y títulos de
// tooltip usan FONT_LARGE (7 px; el ancla y sube 1 px para quedar centrada) y los botones de texto usan
// FONT_SMALL con sombra (FONT_LARGE no entra en 84 px y la "negrita" por doble trazo no se leía).
#define F2 FONT_LARGE
#define txt_w font_text_width
#define txt font_draw

// ------------------------------------------------------------------ formas (Spec §5)
static int phase(uint32_t now) { return (int)(now / 150) & 1; }
static void ants(PxCanvas *c, PxRect ring, uint32_t now) {   // ring = rectángulo del anillo, sin esquinas
    px_focus_ants(c, pxr(ring.x + 1, ring.y + 1, ring.w - 2, ring.h - 2), 1, phase(now));
}

static void shape(PxCanvas *c, PxRect r, int k, uint32_t b, uint32_t f, uint32_t l) {
    int x = r.x, y = r.y, w = r.w, h = r.h;
    if (k == 2) {
        px_hline(c, x + 2, y, w - 4, b);
        px_hline(c, x + 2, y + h - 1, w - 4, b);
        px_pset(c, x + 1, y + 1, b); px_pset(c, x + w - 2, y + 1, b);
        px_pset(c, x + 1, y + h - 2, b); px_pset(c, x + w - 2, y + h - 2, b);
        px_vline(c, x, y + 2, h - 4, b);
        px_vline(c, x + w - 1, y + 2, h - 4, b);
        px_hline(c, x + 2, y + 1, w - 4, l ? l : f);
        px_fill(c, pxr(x + 1, y + 2, w - 2, h - 4), f);
        px_hline(c, x + 2, y + h - 2, w - 4, f);
    } else {
        px_hline(c, x + 1, y, w - 2, b);
        px_hline(c, x + 1, y + h - 1, w - 2, b);
        px_vline(c, x, y + 1, h - 2, b);
        px_vline(c, x + w - 1, y + 1, h - 2, b);
        px_fill(c, pxr(x + 1, y + 1, w - 2, h - 2), f);
        if (l) px_hline(c, x + 1, y + 1, w - 2, l);
    }
}

typedef struct { uint32_t b, f, l, t; int sh, dy; } Look;
static Look look(int st, int pending) {
    Look k = {PX_BLACK, PX_DARK, PX_PARCH, pending ? PX_WHITE : PX_TEXT, 1, 0};
    if (st & UI_ST_DISABLED) { k.f = PX_SHADOW; k.l = PX_DARK; k.t = PX_GRAY; k.sh = 0; return k; }
    if (pending) k.b = PX_GOLD;
    if (st & UI_ST_PRESSED) { k.f = PX_GRAY; k.l = PX_SHADOW; k.t = PX_WHITE; k.dy = 1; }
    else if (st & UI_ST_HOVER) { k.f = PX_PARCH; k.l = PX_WHITE; k.t = PX_BLACK; k.sh = 0; }
    return k;
}

static void text_button(PxCanvas *c, PxRect r, int k, FontId f, const char *label, int st, int pending, uint32_t now) {
    Look L = look(st, pending);
    shape(c, r, k, L.b, L.f, L.l);
    int tw = txt_w(f, label), cap = font_cap(f);
    txt(c, f, r.x + (r.w - tw) / 2, r.y + (r.h - cap) / 2 + L.dy, label, L.t, L.sh ? TX_SHADOW : 0);
    if (st & UI_ST_FOCUS) ants(c, pxr(r.x - 2, r.y - 2, r.w + 4, r.h + 4), now);
}

static void icon_button(PxCanvas *c, PxRect r, const char *icon, int ix, int iy, int st, int is_x, uint32_t now) {
    Look L = look(st, 0);
    shape(c, r, 1, L.b, L.f, L.l);
    uint32_t plus = (st & UI_ST_DISABLED) ? PX_GRAY : (st & (UI_ST_HOVER | UI_ST_PRESSED)) ? PX_WHITE : PX_TEXT;
    if (is_x) {
        uint32_t sh[1] = {PX_BLACK}, fg[1] = {plus};
        bmp(c, ix + 1, iy + 1 + L.dy, icon, "#", sh);
        bmp(c, ix, iy + L.dy, icon, "#", fg);
    } else {
        uint32_t cols[3] = {PX_BLACK, PX_BLACK, plus};
        bmp(c, ix, iy + L.dy, icon, "#o+", cols);
    }
    if (st & UI_ST_FOCUS) ants(c, pxr(r.x - 2, r.y - 2, r.w + 4, r.h + 4), now);
}

// Estilo rollo (kakejiku): las barras bajas son rodillos de madera con perillas en las puntas;
// los marcos grandes son seda de brocado con una trama fina.
static void bar(PxCanvas *c, PxRect r) {
    if (r.h <= 40) {
        px_fill(c, r, PX_MAROON);
        px_hline(c, r.x, r.y, r.w, PX_RUST);                         // brillo arriba
        px_hline(c, r.x, r.y + 1, r.w, PX_RUST);
        px_hline(c, r.x, r.y + r.h - 1, r.w, PX_BLACK);              // sombra abajo
        for (int y = r.y + 3; y < r.y + r.h - 2; y += 3)             // vetas
            for (int x = r.x; x < r.x + r.w; x++)
                if (px_hash((uint32_t)x / 7, (uint32_t)y, 91u) % 5 == 0) px_pset(c, x, y, PX_RUST);
        for (int k = 0; k < 2; k++) {                                // perillas de los extremos
            int kx = k ? r.x + r.w - 7 : r.x;
            px_fill(c, pxr(kx, r.y, 7, r.h), PX_BLACK);
            px_fill(c, pxr(kx + 1, r.y + 1, 5, r.h - 2), PX_GOLD);
            px_vline(c, kx + 3, r.y + 2, r.h - 4, PX_RUST);
        }
    } else {
        px_fill(c, r, 0xFF2c3646u);                                  // seda indigo
        for (int y = r.y; y < r.y + r.h; y++)
            for (int x = r.x; x < r.x + r.w; x++)
                if ((x + y) % 6 == 0 || (x - y + 600) % 6 == 0) px_pset(c, x, y, 0xFF3a4659u);
        px_outline(c, px_inset(r, 1), PX_GOLD);                      // hilo dorado
    }
}

// Sello rojo (hanko) con un "山" estilizado.
static void hanko(PxCanvas *c, int x, int y) {
    px_fill(c, pxr(x, y, 13, 13), PX_BRASS);
    px_outline(c, pxr(x, y, 13, 13), 0xFF7f2a1du);
    uint32_t p = PX_PARCH;
    px_vline(c, x + 6, y + 2, 8, p);
    px_vline(c, x + 3, y + 5, 5, p);
    px_vline(c, x + 9, y + 5, 5, p);
    px_hline(c, x + 3, y + 10, 7, p);
}

static void separator(PxCanvas *c, int x, int y, int w, uint32_t line, uint32_t diamond) {
    int cx = x + w / 2;
    px_hline(c, x, y, cx - 4 - x, line);
    px_hline(c, cx + 5, y, x + w - (cx + 5), line);
    px_diamond(c, cx, y, 2, diamond);
}

static void pip(PxCanvas *c, int x, int y, uint32_t col) {
    px_hline(c, x + 1, y, 3, PX_BLACK);
    px_hline(c, x + 1, y + 4, 3, PX_BLACK);
    px_vline(c, x, y + 1, 3, PX_BLACK);
    px_vline(c, x + 4, y + 1, 3, PX_BLACK);
    px_fill(c, pxr(x + 1, y + 1, 3, 3), col);
}

// ------------------------------------------------------------------ estado y acciones
static int widget_state(Ui *ui, int id, PxRect r, int disabled) {
    if (disabled) return UI_ST_DISABLED;
    int st = 0;
    if (ui->hot == id) st |= UI_ST_HOVER;
    if (ui->active == id && px_in(r, ui->mx, ui->my)) st |= UI_ST_PRESSED;
    if (ui->focus == id && ui->focus_visible) st |= UI_ST_FOCUS;
    return st;
}

static int query_run(void) {
    wchar_t cmd[MAX_PATH + 4];
    if (!fc_startup_command(cmd, MAX_PATH + 4)) return FC_RUN_ERROR;
    return fc_startup_query(FC_RUN_VALUE, cmd, NULL, 0);
}

static int is_dirty(const App *a) {
    int run_ok = a->cfg.start_with_windows ? a->run_state == FC_RUN_MATCH : a->run_state == FC_RUN_ABSENT;
    return !fc_cfg_equal(&a->cfg, &a->saved) || !a->file_exists || !run_ok;
}

static int fondo_running(const App *a) { return a->fake_fondo >= 0 ? a->fake_fondo : fc_fondo_find() != NULL; }

static void launch(App *a, uint32_t now) {
    if (!fc_fondo_launch(a->dry_sys)) fc_log("no pude lanzar fondo.exe");
    a->n_launch++;
    a->last_poll = now - (POLL_MS - 700);   // volver a mirar pronto
}

// Aplicar (7.4): config.ini + clave Run + recargar o lanzar.
static void do_apply(App *a, uint32_t now) {
    fc_cfg_clamp(&a->cfg);
    for (int v = 0; v < 2; v++)                       // un solo fondo a la vez
        if (v != a->cfg.variant && (a->dry_sys || fc_fondo_find_v(v))) fc_fondo_quit_v(v, a->dry_sys);
    if (a->cfg.variant != fc_variant) a->running = a->fake_fondo >= 0 ? a->fake_fondo : fc_fondo_find_v(a->cfg.variant) != NULL;
    fc_variant = a->cfg.variant;                      // exe, clave Run y mensajes del fondo elegido
    int ok = fc_cfg_save(&a->cfg, a->path, a->dry_cfg);
    if (!ok) fc_log("no pude escribir config.ini");
    if (ok) {
        a->n_save++;
        ok = fc_startup_apply(FC_RUN_VALUE, a->cfg.start_with_windows, a->dry_sys);
        if (!ok) fc_log("no pude actualizar la clave Run");
    }
    if (!ok) { a->error = 1; a->saved_until = 0; return; }
    a->saved = a->cfg;
    a->file_exists = 1;
    a->error = 0;
    a->run_state = a->dry_sys ? (a->cfg.start_with_windows ? FC_RUN_MATCH : FC_RUN_ABSENT) : query_run();
    a->saved_until = now + SAVED_MS;
    if (a->running) { fc_fondo_reload(a->dry_sys); a->n_reload++; }
    else launch(a, now);
}

static void roll_seed(App *a) {
    static const char AL[] = "abcdefghijkmnpqrstuvwxyz23456789";
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    char s[9];
    do {
        for (int i = 0; i < 8; i++) s[i] = AL[px_hash((uint32_t)q.LowPart, (uint32_t)i, (uint32_t)q.HighPart ^ GetTickCount()) % 32u];
        s[8] = 0;
        q.LowPart += 7919;
    } while (!strcmp(s, a->cfg.seed));
    fc_cfg_set_seed(&a->cfg, s);
}

// Nuevo paisaje (7.4): azar -> WM_COMMAND 3 (o lanzar); semilla fija -> tirar otra y aplicar.
static void new_landscape(App *a, uint32_t now) {
    if (!a->cfg.seed[0]) {
        if (a->running) { fc_fondo_new_landscape(a->dry_sys); a->n_new++; }
        else launch(a, now);
    } else {
        roll_seed(a);
        do_apply(a, now);
    }
}

static void main_action(App *a, uint32_t now) {   // APLICAR / ENCENDER
    if (is_dirty(a) || a->error) do_apply(a, now);
    else if (!a->running) launch(a, now);
}

// ------------------------------------------------------------------ tooltip (Spec §4 TOOLTIP, §10)
static void tip(App *a, Ui *ui, int id, PxRect hit, PxRect anchor, int zone, const char *title, const char *func, const char *amb) {
    int inside = px_in(hit, ui->mx, ui->my);
    if (inside) a->hov_cand = id;
    int hover = inside && a->hov_id == id && !a->tip_hidden && !ui->mdown && ui->now - a->hov_since >= UI_TIP_DELAY;
    int forced = ui->tip_forced && ui->focus == id;
    if (hover || forced) {
        a->tip_id = id; a->tip_zone = zone; a->tip_anchor = anchor;
        a->tip_title = title; a->tip_func = i18n_tr(func); a->tip_amb = i18n_tr(amb);
    }
}

static int split_lines(char *buf, char **out, int max) {
    int n = 0;
    char *p = buf;
    while (*p && n < max) {
        out[n++] = p;
        char *e = strchr(p, '\n');
        if (!e) break;
        *e = 0;
        p = e + 1;
    }
    return n;
}

static void draw_tip(App *a, PxCanvas *c) {
    char fb[512] = "", ab[512] = "";
    char *ln[16];
    uint32_t lc[16];
    int n = 0, w = txt_w(F2, a->tip_title);
    if (a->tip_func) {
        font_wrap(FONT_SMALL, a->tip_func, 140, fb, sizeof fb);
        int k = split_lines(fb, ln + n, 8);
        for (int i = 0; i < k; i++) lc[n + i] = PX_TEXT;
        n += k;
    }
    if (a->tip_amb) {
        font_wrap(FONT_SMALL, a->tip_amb, 140, ab, sizeof ab);
        int k = split_lines(ab, ln + n, 8);
        for (int i = 0; i < k; i++) lc[n + i] = PX_PARCH;
        n += k;
    }
    for (int i = 0; i < n; i++) { int lw = font_text_width(FONT_SMALL, ln[i]); if (lw > w) w = lw; }
    w += 10;
    if (w > 150) w = 150;
    int h = 30 + 9 * (n > 0 ? n - 1 : 0), x, y;
    PxRect an = a->tip_anchor;
    if (a->tip_zone == Z_RIGHT) {
        x = 224 - w;
        y = an.y;
        if (y > 222 - h) y = 222 - h;
        if (y < 22) y = 22;
    } else {
        y = a->tip_zone == Z_BOTTOM ? 222 - h : 22;
        x = an.x + an.w / 2 - w / 2;
        if (x > 398 - w) x = 398 - w;
        if (x < 2) x = 2;
    }
    a->tip_rect = pxr(x, y, w, h);
    px_fill(c, a->tip_rect, PX_BLACK);
    px_outline(c, a->tip_rect, PX_GRAY);
    txt(c, F2, x + (w - txt_w(F2, a->tip_title)) / 2, y + 4, a->tip_title, PX_WHITE, 0);
    separator(c, x + 5, y + 14, w - 10, PX_DARK, PX_PARCH);
    for (int i = 0; i < n; i++) font_draw(c, FONT_SMALL, x + 5, y + 20 + 9 * i, ln[i], lc[i], 0);
}

// ------------------------------------------------------------------ piezas del diseño
static void draw_slider(PxCanvas *c, int y0, const char *label, const char *readout, double t, const double *ticks,
                        int nt, int def, const UiRange *g, int st, int drag, uint32_t now) {
    int hov = (st & UI_ST_HOVER) || drag;
    font_draw(c, FONT_SMALL, 236, y0 + 3, label, hov ? PX_MAROON : PX_BLACK, 0);
    px_hline(c, 353, y0, 34, PX_BLACK);
    px_fill(c, pxr(352, y0 + 1, 36, 9), PX_BLACK);
    px_hline(c, 353, y0 + 10, 34, PX_BLACK);
    font_draw(c, FONT_SMALL, 352 + (36 - font_text_width(FONT_SMALL, readout)) / 2, y0 + 3, readout, drag ? PX_WHITE : PX_TEXT, 0);
    int gy = y0 + 15, cx = 239 + (int)floor(145 * t + 0.5);
    px_hline(c, 239, gy, 146, PX_BLACK);
    px_hline(c, 239, gy + 2, 146, PX_BLACK);
    px_hline(c, 239, gy + 1, cx - 239 + 1, PX_BRASS);
    px_hline(c, cx + 1, gy + 1, 384 - cx, PX_DARK);
    px_diamond(c, 237, gy + 1, 1, PX_DARK);
    px_diamond(c, 386, gy + 1, 1, PX_DARK);
    for (int i = 0; i < nt; i++) {
        int nx = 239 + (int)floor(145 * ui_range_t(g, ticks[i]) + 0.5);
        px_vline(c, nx, y0 + 19, 2, i == def ? PX_ORANGE : PX_DARK);
    }
    shape(c, pxr(cx - 2, y0 + 12, 5, 9), 1, PX_BLACK, hov ? PX_GOLD : PX_BRASS, hov ? PX_WHITE : PX_GOLD);
    if (st & UI_ST_FOCUS) ants(c, pxr(cx - 4, y0 + 10, 9, 13), now);
}

static void draw_toggle(PxCanvas *c, int x, int y, const char *label, int on, int st, uint32_t now) {
    int hov = st & UI_ST_HOVER;
    px_outline(c, pxr(x, y, 8, 8), hov ? PX_WHITE : PX_BLACK);
    px_fill(c, pxr(x + 1, y + 1, 6, 6), on ? PX_BRASS : PX_SHADOW);
    px_hline(c, x + 1, y + 1, 6, on ? PX_GOLD : PX_BLACK);
    px_vline(c, x + 1, y + 2, 5, on ? PX_GOLD : PX_BLACK);
    font_draw(c, FONT_SMALL, x + 12, y + 2, label, hov ? PX_MAROON : PX_BLACK, 0);
    if (st & UI_ST_FOCUS) ants(c, pxr(232, y - 4, 158, 16), now);
}

static void draw_seed_field(PxCanvas *c, Ui *ui, const char *seed, int st, uint32_t now) {
    PxRect r = pxr(236, 41, 112, 14);
    px_outline(c, r, PX_BLACK);
    px_fill(c, px_inset(r, 1), PX_SHADOW);
    px_hline(c, 237, 53, 110, PX_DARK);
    px_vline(c, 346, 42, 11, PX_DARK);
    int editing = ui->focus == ID_SEED;
    PxRect old = px_set_clip(c, pxr(240, 42, 104, 12));
    if (!seed[0]) { if (!editing) font_draw(c, FONT_SMALL, 240, 46, "AL AZAR", PX_GRAY, 0); }
    else font_draw(c, FONT_SMALL, 240 - (editing ? ui->tf_scroll : 0), 46, seed, PX_TEXT, 0);
    c->clip = old;
    if (editing && ((now - ui->tf_blink0) / 530) % 2 == 0) {
        char tmp[FC_SEED_BUF];
        int off = utf8_offset(seed, ui->tf_cursor);
        memcpy(tmp, seed, (size_t)off);
        tmp[off] = 0;
        int x = 239 + (off ? font_text_width(FONT_SMALL, tmp) + 1 : 0) - ui->tf_scroll;
        if (x < 239) x = 239;
        if (x > 344) x = 344;
        px_vline(c, x, 45, 7, PX_WHITE);
    }
    if (st & UI_ST_FOCUS) ants(c, pxr(234, 39, 116, 18), now);
}

static void draw_retreat(PxCanvas *c, int st, int holding, double p, int flash, uint32_t now) {
    int dis = st & UI_ST_DISABLED, hov = (st & UI_ST_HOVER) && !dis;
    font_draw(c, FONT_SMALL, 316, 240, "RETIRARSE", dis ? PX_RUST : PX_TEXT, dis ? 0 : TX_SHADOW);
    if (!dis && (flash || (holding && p > 0))) {
        for (int y = 228; y < 256; y++)
            for (int x = 362; x < 390; x++) {
                double dx = x + 0.5 - 376, dy = y + 0.5 - 242, d = sqrt(dx * dx + dy * dy);
                if (d < 12 || d >= 14) continue;
                double ang = atan2(dx, -dy);
                if (ang < 0) ang += 2 * M_PI;
                px_pset(c, x, y, flash ? PX_WHITE : ang < p * 2 * M_PI ? PX_GOLD : PX_SHADOW);
            }
    }
    uint32_t fill = dis ? PX_SHADOW : hov ? PX_ORANGE : PX_MAROON, arc = dis ? fill : hov ? PX_GOLD : PX_ORANGE;
    for (int y = 232; y < 252; y++)
        for (int x = 366; x < 386; x++) {
            double dx = x + 0.5 - 376, dy = y + 0.5 - 242, d = sqrt(dx * dx + dy * dy);
            if (d >= 10) continue;
            px_pset(c, x, y, d >= 9 ? PX_BLACK : (d >= 8 && dy < -3) ? arc : fill);
        }
    int ry = 237 + (holding ? 1 : 0);
    uint32_t k[1] = {PX_BLACK}, fg[1] = {dis ? PX_GRAY : hov ? PX_WHITE : PX_TEXT};
    if (!dis) bmp(c, 371, ry + 1, IC_CORREDOR, "#", k);
    bmp(c, 370, ry, IC_CORREDOR, "#", fg);
    if (st & UI_ST_FOCUS) ants(c, pxr(311, 227, 80, 30), now);
}

static void draw_eltoggle(PxCanvas *c, int x, int y, const char *label, int on, int st, uint32_t now) {
    int hov = st & UI_ST_HOVER;
    px_outline(c, pxr(x, y, 8, 8), hov ? PX_WHITE : PX_BLACK);
    px_fill(c, pxr(x + 1, y + 1, 6, 6), on ? PX_BRASS : PX_SHADOW);
    px_hline(c, x + 1, y + 1, 6, on ? PX_GOLD : PX_BLACK);
    px_vline(c, x + 1, y + 2, 5, on ? PX_GOLD : PX_BLACK);
    font_draw(c, FONT_SMALL, x + 11, y + 2, label, hov ? PX_MAROON : on ? PX_BLACK : PX_GRAY, 0);
    if (st & UI_ST_FOCUS) ants(c, pxr(x - 3, y - 3, 100, 14), now);
}

static void draw_preview(PxCanvas *c, const char *seed) {
    px_outline(c, pxr(4, 24, 220, 196), PX_BLACK);
    bar(c, pxr(5, 25, 218, 194));
    px_outline(c, pxr(9, 29, 210, 186), PX_BLACK);
    px_fill(c, pxr(10, 30, 208, 184), PX_PARCH);
    // placa con la semilla
    shape(c, pxr(44, 207, 140, 12), 1, PX_BLACK, PX_BRASS, PX_NONE);
    char t[FC_SEED_BUF + 32];
    if (!seed[0]) snprintf(t, sizeof t, "SEMILLA: AL AZAR");
    else {
        snprintf(t, sizeof t, "SEMILLA: %s", seed);
        if (font_text_width(FONT_SMALL, t) > 132) {
            int n = utf8_count(seed);
            while (n > 0) {
                char cut[FC_SEED_BUF];
                int off = utf8_offset(seed, --n);
                memcpy(cut, seed, (size_t)off);
                cut[off] = 0;
                snprintf(t, sizeof t, "SEMILLA: %s…", cut);
                if (font_text_width(FONT_SMALL, t) <= 132) break;
            }
        }
    }
    font_draw(c, FONT_SMALL, 44 + (140 - font_text_width(FONT_SMALL, t)) / 2, 211, t, PX_BLACK, 0);
}

// ------------------------------------------------------------------ cuadro
#define LOGIC(ui, a, call) do { PxCanvas *real_ = (ui)->c; (ui)->c = &(a)->scratch; call; (ui)->c = real_; } while (0)

static void close_window(App *a) { if (a->hwnd) PostMessageW(a->hwnd, WM_CLOSE, 0, 0); }

static void frame_ui(Ui *ui, App *a) {
    PxCanvas *c = ui->c;
    uint32_t now = ui->now;
    a->hov_cand = 0;
    g_lang_en = a->cfg.lang;
    a->tip_id = 0;

    // sondeo del fondo cada 2 s
    if (!a->polled || now - a->last_poll >= POLL_MS) { a->running = fondo_running(a); a->last_poll = now; a->polled = 1; }
    // fin del destello de RETIRARSE -> WM_COMMAND 1
    if (a->quit_at && (int32_t)(now - a->quit_at) >= 0) {
        fc_fondo_quit(a->dry_sys);
        a->n_quit++;
        a->quit_at = 0;
        a->running = 0;
        a->last_poll = now - (POLL_MS - 700);
    }

    bar(c, pxr(0, 0, CW, CH));                     // fondo: seda de brocado (montaje del rollo)
    int dirty = is_dirty(a);

    // ---- barra de título
    bar(c, pxr(1, 1, 398, 18));
    px_hline(c, 0, 19, 400, PX_BLACK);
    uint32_t mc[2] = {PX_BLACK, PX_PARCH};
    bmp(c, 6, 7, IC_MONTE, "#+", mc);
    const char *TITLE = a->saved.variant ? "FONDO · NIPPON" : "FONDO · SHAN SHUI";
    txt(c, F2, 19, 6, TITLE, PX_TEXT, TX_SHADOW);
    if (dirty) txt(c, F2, 19 + txt_w(F2, TITLE) + txt_w(F2, " "), 6, "*", PX_GOLD, TX_SHADOW);
    hanko(c, 364, 3);
    {   // idioma: ES / EN (se guarda en config.ini con APLICAR, se ve al instante)
        PxRect rl = pxr(340, 4, 20, 12);
        int clk = 0;
        LOGIC(ui, a, clk = ui_button(ui, ID_LANG, rl, NULL, NULL, 0));
        if (clk) a->cfg.lang = !a->cfg.lang;
        text_button(c, rl, 1, FONT_SMALL, a->cfg.lang ? "EN" : "ES", widget_state(ui, ID_LANG, rl, 0), 0, now);
    }
    PxRect rclose = pxr(383, 4, 13, 12);
    int clicked;
    LOGIC(ui, a, clicked = ui_button(ui, ID_CLOSE, rclose, NULL, NULL, 0));
    for (int i = 0; i < ui->nitems; i++)   // fuera del orden de Tab
        if (ui->items[i].id == ID_CLOSE) { memmove(&ui->items[i], &ui->items[i + 1], sizeof ui->items[0] * (size_t)(ui->nitems - i - 1)); ui->nitems--; i--; }
    icon_button(c, rclose, IC_X, 387, 7, widget_state(ui, ID_CLOSE, rclose, 0) & ~UI_ST_FOCUS, 1, now);
    tip(a, ui, ID_CLOSE, rclose, rclose, Z_TITLE, "CERRAR", "CERRAR LA VENTANA. EL FONDO SIGUE.", NULL);
    if (clicked) close_window(a);
    ui->caption = pxr(1, 1, 381, 18);

    // ---- vista previa
    draw_preview(c, a->cfg.seed);
    {   // panel izquierdo: que fondo y que elementos
        txt(c, F2, 14, 34, "FONDO", PX_MAROON, 0);
        static const char *VN[2] = {"CHINA", "JAP\xC3\x93N"};   // "JAPÓN" (escape: evita formas Unicode descompuestas)
        for (int v = 0; v < 2; v++) {
            PxRect r = pxr(66 + v * 76, 32, 70, 13);
            int clk = 0;
            LOGIC(ui, a, clk = ui_button(ui, ID_VAR0 + v, r, NULL, NULL, 0));
            if (clk) a->cfg.variant = v;
            int st = widget_state(ui, ID_VAR0 + v, r, 0);
            if (a->cfg.variant == v) st |= UI_ST_PRESSED;
            text_button(c, r, 2, FONT_SMALL, VN[v], st, 0, now);
        }
        separator(c, 14, 51, 200, PX_DARK, PX_MAROON);
        txt(c, F2, 14, 56, "ELEMENTOS", PX_MAROON, 0);
        int v = a->cfg.variant, n;
        const FcElem *E = fc_elems(v, &n);
        for (int i = 0; i < n; i++) {
            int x = 16 + (i / 10) * 102, y = 68 + (i % 10) * 13;
            PxRect r = pxr(x - 2, y - 2, 98, 12);
            int on = !(a->cfg.el_off[v] & (1u << i)), was = on;
            LOGIC(ui, a, ui_toggle(ui, ID_EL0 + i, r, NULL, &on, 0));
            if (on != was) { if (on) a->cfg.el_off[v] &= ~(1u << i); else a->cfg.el_off[v] |= 1u << i; }
            draw_eltoggle(c, x, y, E[i].label, on, widget_state(ui, ID_EL0 + i, r, 0), now);
        }
    }
    // ---- panel de controles
    px_outline(c, pxr(228, 24, 168, 196), PX_BLACK);
    px_outline(c, pxr(229, 25, 166, 194), PX_BRASS);
    PxRect parch = pxr(230, 26, 164, 192);
    px_fill(c, parch, PX_PARCH);
    px_noise(c, parch, 37, 12, PX_GRAY);
    px_noise(c, parch, 38, 3, PX_RUST);

    txt(c, F2, 236, 32, "SEMILLA", PX_MAROON, 0);
    PxRect rseed = pxr(236, 41, 112, 14);
    int sres;
    LOGIC(ui, a, sres = ui_textfield(ui, ID_SEED, rseed, a->cfg.seed, FC_SEED_BUF, FC_SEED_MAX, NULL, 0));
    draw_seed_field(c, ui, a->cfg.seed, widget_state(ui, ID_SEED, rseed, 0), now);
    tip(a, ui, ID_SEED, rseed, rseed, Z_RIGHT, "SEMILLA", "MISMO NOMBRE, MISMO PAISAJE. VACÍA: UNO DISTINTO CADA VEZ QUE ARRANCA.", NULL);

    PxRect rdie = pxr(352, 41, 16, 14), rforget = pxr(372, 41, 16, 14);
    LOGIC(ui, a, clicked = ui_button(ui, ID_DIE, rdie, NULL, NULL, 0));
    if (clicked) roll_seed(a);
    icon_button(c, rdie, IC_DADO, 356, 44, widget_state(ui, ID_DIE, rdie, 0), 0, now);
    tip(a, ui, ID_DIE, rdie, rdie, Z_RIGHT, "TIRAR", "TIRÁ UNA SEMILLA NUEVA AL AZAR.", NULL);
    int fdis = !a->cfg.seed[0];
    LOGIC(ui, a, clicked = ui_button(ui, ID_FORGET, rforget, NULL, NULL, fdis ? UI_DISABLED : 0));
    if (clicked) a->cfg.seed[0] = 0;
    fdis = !a->cfg.seed[0];
    icon_button(c, rforget, IC_OLVIDAR, 376, 44, widget_state(ui, ID_FORGET, rforget, fdis), 0, now);
    tip(a, ui, ID_FORGET, rforget, rforget, Z_RIGHT, "OLVIDAR", "BORRÁ LA SEMILLA.", "EL PAISAJE VUELVE A SER SORPRESA.");

    separator(c, 236, 61, 152, PX_DARK, PX_MAROON);

    // sliders
    char rd[24];
    int res, submit = (sres & UI_SUBMIT) != 0;
    {   // MARCHA
        int y0 = 66;
        PxRect r = pxr(237, y0 + 11, 150, 11), hit = pxr(236, y0 + 11, 152, 11);
        LOGIC(ui, a, res = ui_slider(ui, ID_SPEED, r, &a->cfg.speed, &R_SPEED, 0));
        submit |= (res & UI_SUBMIT) != 0;
        if (a->cfg.speed == 0) strcpy(rd, "QUIETO");
        else snprintf(rd, sizeof rd, "%.0f", a->cfg.speed);
        draw_slider(c, y0, "MARCHA", rd, ui_range_t(&R_SPEED, a->cfg.speed), SPEED_TICKS, 6, 1, &R_SPEED,
                    widget_state(ui, ID_SPEED, hit, 0), ui->drag_id == ID_SPEED, now);
        tip(a, ui, ID_SPEED, hit, pxr(236, y0, 152, 22), Z_RIGHT, "MARCHA", "QUÉ TAN RÁPIDO AVANZA EL MUNDO (0–400).", "EN 0, EL PAISAJE SE QUEDA QUIETO.");
    }
    {   // RITMO: RePág/AvPág saltan a la muesca siguiente
        int y0 = 90;
        double fps = a->cfg.fps;
        if (ui->focus == ID_FPS) {
            if (ui_key_pressed(ui, VK_PRIOR, 0)) {
                for (int i = 0; i < 5; i++) if (FPS_TICKS[i] > fps + 1e-9) { fps = FPS_TICKS[i]; break; }
                ui->focus_visible = 1;
            }
            if (ui_key_pressed(ui, VK_NEXT, 0)) {
                for (int i = 4; i >= 0; i--) if (FPS_TICKS[i] < fps - 1e-9) { fps = FPS_TICKS[i]; break; }
                ui->focus_visible = 1;
            }
        }
        PxRect r = pxr(237, y0 + 11, 150, 11), hit = pxr(236, y0 + 11, 152, 11);
        LOGIC(ui, a, res = ui_slider(ui, ID_FPS, r, &fps, &R_FPS, 0));
        submit |= (res & UI_SUBMIT) != 0;
        a->cfg.fps = (int)floor(fps + 0.5);
        snprintf(rd, sizeof rd, "%d", a->cfg.fps);
        draw_slider(c, y0, "RITMO", rd, ui_range_t(&R_FPS, a->cfg.fps), FPS_TICKS, 5, 2, &R_FPS,
                    widget_state(ui, ID_FPS, hit, 0), ui->drag_id == ID_FPS, now);
        tip(a, ui, ID_FPS, hit, pxr(236, y0, 152, 22), Z_RIGHT, "RITMO", "CUADROS POR SEGUNDO (5–120).", "MÁS ES MÁS SUAVE Y GASTA MÁS.");
    }
    {   // CERCANÍA
        int y0 = 114;
        PxRect r = pxr(237, y0 + 11, 150, 11), hit = pxr(236, y0 + 11, 152, 11);
        LOGIC(ui, a, res = ui_slider(ui, ID_ZOOM, r, &a->cfg.zoom, &R_ZOOM, 0));
        submit |= (res & UI_SUBMIT) != 0;
        snprintf(rd, sizeof rd, "X%.1f", a->cfg.zoom);
        draw_slider(c, y0, "CERCANÍA", rd, ui_range_t(&R_ZOOM, a->cfg.zoom), ZOOM_TICKS, 6, 1, &R_ZOOM,
                    widget_state(ui, ID_ZOOM, hit, 0), ui->drag_id == ID_ZOOM, now);
        tip(a, ui, ID_ZOOM, hit, pxr(236, y0, 152, 22), Z_RIGHT, "CERCANÍA", "QUÉ TAN CERCA MIRÁS LAS MONTAÑAS (X0.5–X3.0).", "CAMBIARLO REHACE EL PAISAJE.");
    }

    separator(c, 236, 142, 152, PX_DARK, PX_MAROON);
    uint32_t fc[4] = {PX_ORANGE, PX_GOLD, PX_WHITE, PX_RUST};
    bmp(c, 236, 150, IC_FOGATA, "*#Y+", fc);
    txt(c, F2, 246, 150, "COSTUMBRES", PX_MAROON, 0);

    PxRect rp = pxr(234, 161, 154, 12), rs = pxr(234, 175, 154, 12);
    LOGIC(ui, a, ui_toggle(ui, ID_PAUSE, rp, NULL, &a->cfg.pause_when_covered, 0));
    draw_toggle(c, 236, 163, "QUIETO SI LO TAPAN", a->cfg.pause_when_covered, widget_state(ui, ID_PAUSE, rp, 0), now);
    tip(a, ui, ID_PAUSE, rp, pxr(236, 163, 8, 8), Z_RIGHT, "QUIETO SI LO TAPAN", "SE DETIENE CUANDO UNA VENTANA CUBRE CASI TODA LA PANTALLA.", "NADIE MIRA, NADIE CAMINA.");
    LOGIC(ui, a, ui_toggle(ui, ID_START, rs, NULL, &a->cfg.start_with_windows, 0));
    draw_toggle(c, 236, 177, "DESPERTAR CON WINDOWS", a->cfg.start_with_windows, widget_state(ui, ID_START, rs, 0), now);
    tip(a, ui, ID_START, rs, pxr(236, 177, 8, 8), Z_RIGHT, "DESPERTAR CON WINDOWS", "ARRANCA SOLO AL INICIAR SESIÓN.", NULL);

    FcConfig def;
    fc_cfg_defaults(&def);
    PxRect rreset = pxr(303, 196, 85, 13);
    int rdis = fc_cfg_equal(&a->cfg, &def);
    LOGIC(ui, a, clicked = ui_button(ui, ID_RESET, rreset, NULL, NULL, rdis ? UI_DISABLED : 0));
    if (clicked) { a->cfg = def; rdis = 1; }
    text_button(c, rreset, 1, FONT_SMALL, "COMO AL PRINCIPIO", widget_state(ui, ID_RESET, rreset, rdis), 0, now);
    tip(a, ui, ID_RESET, rreset, rreset, Z_RIGHT, "COMO AL PRINCIPIO", "VOLVÉ A LOS VALORES DE SIEMPRE.", "FALTA APLICAR.");

    // ---- barra inferior
    dirty = is_dirty(a);
    px_hline(c, 0, 224, 400, PX_BLACK);
    bar(c, pxr(1, 225, 398, 34));

    PxRect rnew = pxr(144, 232, 84, 19);
    LOGIC(ui, a, clicked = ui_button(ui, ID_NEW, rnew, NULL, NULL, 0));
    if (clicked) new_landscape(a, now);
    text_button(c, rnew, 2, FONT_SMALL, "NUEVO PAISAJE", widget_state(ui, ID_NEW, rnew, 0), 0, now);
    if (a->cfg.seed[0]) tip(a, ui, ID_NEW, rnew, rnew, Z_BOTTOM, "NUEVO PAISAJE", "TIRÁ OTRA SEMILLA Y QUEDATE CON ELLA.", dirty ? "TAMBIÉN GUARDA LO QUE CAMBIASTE." : NULL);
    else tip(a, ui, ID_NEW, rnew, rnew, Z_BOTTOM, "NUEVO PAISAJE", "OTRO PAISAJE AHORA. TU CONFIGURACIÓN NO CAMBIA.", NULL);

    dirty = is_dirty(a);
    PxRect rapply = pxr(232, 232, 58, 19);
    int men = !a->running || dirty || a->error;
    const char *mlabel = a->running ? "APLICAR" : "ENCENDER";
    LOGIC(ui, a, clicked = ui_button(ui, ID_APPLY, rapply, NULL, NULL, men ? 0 : UI_DISABLED));
    if (clicked) main_action(a, now);
    dirty = is_dirty(a);
    men = !a->running || dirty || a->error;
    mlabel = a->running ? "APLICAR" : "ENCENDER";
    text_button(c, rapply, 2, FONT_SMALL, mlabel, widget_state(ui, ID_APPLY, rapply, !men), men, now);
    if (!a->running) tip(a, ui, ID_APPLY, rapply, rapply, Z_BOTTOM, "ENCENDER", "GUARDÁ Y DESPERTÁ AL FONDO.", NULL);
    else tip(a, ui, ID_APPLY, rapply, rapply, Z_BOTTOM, "APLICAR", men ? "GUARDÁ Y MANDÁSELO AL FONDO." : "NADA QUE GUARDAR.", NULL);

    PxRect rret = pxr(312, 228, 78, 28);
    int flashing = a->quit_at != 0, retdis = !a->running || flashing, h = 0;
    if (retdis && ui->hold_id == ID_RETREAT) ui->hold_id = 0;   // un hold viejo no debe completarse al volver el fondo
    LOGIC(ui, a, h = ui_hold_button(ui, ID_RETREAT, rret, NULL, NULL, HOLD_MS, retdis ? UI_DISABLED : 0));
    if (h & UI_HOLD_DONE) { a->quit_at = now + FLASH_MS; flashing = 1; }
    if (h & UI_HOLD_TAP) a->taptip_until = now + TAPTIP_MS;
    if (flashing) ui->anim = 1;
    int holding = ui->hold_id == ID_RETREAT;
    double p = holding ? (double)(now - ui->hold_t0) / HOLD_MS : 0;
    int rst = flashing ? 0 : widget_state(ui, ID_RETREAT, rret, retdis);
    draw_retreat(c, rst, holding, p, flashing, now);
    if (retdis && !flashing) tip(a, ui, ID_RETREAT, rret, rret, Z_BOTTOM, "RETIRARSE", "NO HAY NADA QUE ABANDONAR.", NULL);
    else if (now < a->taptip_until && !holding) {
        tip(a, ui, ID_RETREAT, rret, rret, Z_BOTTOM, "RETIRARSE", "MANTENÉ APRETADO PARA RETIRARTE.", NULL);
        a->tip_id = ID_RETREAT; a->tip_zone = Z_BOTTOM; a->tip_anchor = rret;
        a->tip_title = "RETIRARSE"; a->tip_func = i18n_tr("MANTENÉ APRETADO PARA RETIRARTE."); a->tip_amb = NULL;
        ui->anim = 1;
    } else tip(a, ui, ID_RETREAT, rret, rret, Z_BOTTOM, "RETIRARSE", "MANTENÉ APRETADO PARA APAGAR EL FONDO.", "EL ESCRITORIO QUEDA COMO ERA.");

    // estado (Spec §7)
    const char *s1, *s2 = NULL;
    uint32_t pc, c2 = PX_TEXT;
    if (a->error) { pc = PX_ORANGE; s1 = "NO PUDE GUARDAR."; s2 = "REVISÁ LA CARPETA DE DATOS."; }
    else if (now < a->saved_until) { pc = PX_GREEN; s1 = "GUARDADO."; s2 = "EL CAMINO SIGUE."; }
    else if (!a->running) { pc = PX_SHADOW; s1 = "NO HAY FONDO."; s2 = "EL PAISAJE DUERME."; }
    else { pc = PX_GREEN; s1 = "EL FONDO ANDA."; if (dirty) { s2 = "CAMBIOS SIN GUARDAR."; c2 = PX_GOLD; } }
    pip(c, 8, s2 ? 236 : 240, pc);
    font_draw(c, FONT_SMALL, 16, s2 ? 236 : 240, s1, PX_TEXT, TX_SHADOW);
    if (s2) font_draw(c, FONT_SMALL, 16, 245, s2, c2, TX_SHADOW);

    // atajos (teclas que ningún widget consumió)
    if (ui_key_pressed(ui, 'S', UI_MOD_CTRL) || submit) { if (!a->running || is_dirty(a) || a->error) main_action(a, now); }
    if (!ui_focus_is_text(ui) && ui_key_pressed(ui, 'N', 0)) new_landscape(a, now);
    if (ui_key_pressed(ui, VK_ESCAPE, 0)) {
        if (a->tip_id || ui->tip_forced) { ui->tip_forced = 0; a->tip_hidden = 1; a->taptip_until = 0; a->tip_id = 0; }
        else close_window(a);
    }

    // tooltip arriba de todo
    if (a->hov_cand != a->hov_id) { a->hov_id = a->hov_cand; a->hov_since = now; a->tip_hidden = 0; }
    if (a->hov_id && !a->tip_id && !a->tip_hidden) ui->anim = 1;   // esperando los 400 ms
    if (a->tip_id) draw_tip(a, c);
    else a->tip_rect = pxr(0, 0, 0, 0);

    px_outline(c, pxr(0, 0, CW, CH), PX_BLACK);
    a->dirty = is_dirty(a);
    a->st1 = s1;
    a->main_label = mlabel;
    a->main_enabled = men;
    a->retreat_enabled = !retdis;
}

// ------------------------------------------------------------------ arranque
static void app_load(App *a) {
    fc_cfg_defaults(&a->cfg);
    a->file_exists = fc_cfg_load(&a->cfg, a->path);
    a->saved = a->cfg;
    fc_variant = a->cfg.variant;
    a->run_state = query_run();   // solo lectura
}

static App g_app;
static void win_frame(PxWin *w, void *user) {
    App *a = (App *)user;
    a->hwnd = w->hwnd;
    frame_ui(&w->ui, a);
}

static void render(App *a, Ui *ui, uint32_t t) { ui_begin(ui, t); frame_ui(ui, a); ui_end(ui); }

static int do_shot(App *a, const wchar_t *path, int scale, const char *state, int forcetip) {
    PxCanvas c;
    px_canvas_init(&c, CW, CH);
    Ui ui;
    ui_init(&ui, &c);
    // estado base limpio: lo cargado coincide con el archivo y con la clave Run
    a->saved = a->cfg;
    a->file_exists = 1;
    a->run_state = a->cfg.start_with_windows ? FC_RUN_MATCH : FC_RUN_ABSENT;
    if (a->fake_fondo < 0) a->fake_fondo = strcmp(state, "sinfondo") ? 1 : 0;
    if (!strcmp(state, "sucio") || !strcmp(state, "error")) {
        a->cfg.speed = 45;
        fc_cfg_set_seed(&a->cfg, "Montaña 7");
    }
    if (!strcmp(state, "error")) a->error = 1;
    if (!strcmp(state, "guardado")) a->saved_until = 1000 + SAVED_MS;
    if (forcetip) { ui.focus = ID_SPEED; ui.focus_visible = 1; ui.tip_forced = 1; }
    render(a, &ui, 1000);
    render(a, &ui, 1200);
    int ok = pxwin_shot(&c, scale < 1 ? 1 : scale, 0, path);
    char p8[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, p8, sizeof p8, NULL, NULL);
    fc_log(ok ? "captura guardada: %s (estado %s)" : "no se pudo guardar %s", p8, state);
    px_canvas_free(&c);
    return ok ? 0 : 1;
}

// ------------------------------------------------------------------ selftest (sin ventana)
static int g_fail;
#define CHECK(x) do { if (!(x)) { fc_log("FALLO configapp.c:%d: %s", __LINE__, #x); g_fail++; } } while (0)
static void step(App *a, Ui *ui, uint32_t *t, uint32_t dt) { *t += dt; render(a, ui, *t); }
static void click(App *a, Ui *ui, uint32_t *t, int x, int y) {
    ui_input_mouse(ui, x, y); step(a, ui, t, 16);
    ui_input_button(ui, 1); step(a, ui, t, 16);
    ui_input_button(ui, 0); step(a, ui, t, 16);
}
static void key(App *a, Ui *ui, uint32_t *t, int vk, int mods) {
    ui_input_key(ui, vk, 1, 0, mods); step(a, ui, t, 16);
    ui_input_key(ui, vk, 0, 0, mods); step(a, ui, t, 16);
}
static int knob_ok(PxCanvas *c, int cx, int y0) {   // relleno del tirador (latón, u oro en hover) en cx, y no en cx±3
    uint32_t k = px_get(c, cx, y0 + 18), l = px_get(c, cx - 3, y0 + 18), r = px_get(c, cx + 3, y0 + 18);
    return (k == PX_BRASS || k == PX_GOLD) && l != k && r != k;
}

static int selftest(App *a) {
    wchar_t real_before[1024] = L"", real_after[1024] = L"";
    int rb = fc_startup_query(FC_RUN_VALUE, NULL, real_before, 1024);
    a->dry_sys = 1;       // nunca registro real ni mensajes reales
    a->dry_cfg = 0;       // config.ini real, pero en la ruta temporal de --config
    a->fake_fondo = 1;
    DeleteFileW(a->path);
    app_load(a);
    CHECK(a->file_exists == 0);

    PxCanvas c;
    px_canvas_init(&c, CW, CH);
    Ui ui;
    ui_init(&ui, &c);
    uint32_t t = 1000;
    step(a, &ui, &t, 16);
    // primer arranque: sin config.ini -> sucio, APLICAR pendiente; tiradores por defecto en 271/271/268
    CHECK(a->dirty && a->main_enabled && !strcmp(a->main_label, "APLICAR"));
    CHECK(knob_ok(&c, 271, 66) && knob_ok(&c, 271, 90) && knob_ok(&c, 268, 114));
    CHECK(!strcmp(a->st1, "EL FONDO ANDA.") && a->retreat_enabled);
    // APLICAR con el mouse: escribe la ruta temporal y recarga el fondo (dry)
    click(a, &ui, &t, 232 + 29, 232 + 9);
    CHECK(a->n_save == 1 && a->n_reload == 1 && a->n_launch == 0);
    CHECK(!a->dirty && !a->main_enabled && !strcmp(a->st1, "GUARDADO."));
    FcConfig f;
    fc_cfg_defaults(&f); f.speed = -1;
    CHECK(fc_cfg_load(&f, a->path) == 1 && f.speed == 20 && f.fps == 30);
    step(a, &ui, &t, SAVED_MS + 100);
    CHECK(!strcmp(a->st1, "EL FONDO ANDA."));

    // MARCHA con el mouse: x = 312 es la muesca 100 (t ~ 0.503)
    click(a, &ui, &t, 312, 66 + 16);
    CHECK(fabs(a->cfg.speed - 100) <= 2);
    CHECK(a->dirty && a->main_enabled);
    ui_set_focus(&ui, ID_SPEED);
    key(a, &ui, &t, VK_HOME, 0); CHECK(a->cfg.speed == 0);
    key(a, &ui, &t, VK_RIGHT, 0); CHECK(a->cfg.speed == 1);
    key(a, &ui, &t, VK_PRIOR, 0); CHECK(a->cfg.speed == 11);
    key(a, &ui, &t, VK_END, 0); CHECK(a->cfg.speed == 400);
    CHECK(knob_ok(&c, 384, 66));
    // RITMO: RePág/AvPág a la muesca siguiente, flechas de a 1
    ui_set_focus(&ui, ID_FPS);
    key(a, &ui, &t, VK_PRIOR, 0); CHECK(a->cfg.fps == 60);
    key(a, &ui, &t, VK_PRIOR, 0); CHECK(a->cfg.fps == 120);
    key(a, &ui, &t, VK_NEXT, 0); CHECK(a->cfg.fps == 60);
    key(a, &ui, &t, VK_RIGHT, 0); CHECK(a->cfg.fps == 61);
    // CERCANÍA: paso 0.1
    ui_set_focus(&ui, ID_ZOOM);
    key(a, &ui, &t, VK_RIGHT, 0); CHECK(fabs(a->cfg.zoom - 1.1) < 1e-6);
    // toggles: clic en la fila y Espacio con foco
    click(a, &ui, &t, 300, 167); CHECK(a->cfg.pause_when_covered == 0);
    ui_set_focus(&ui, ID_START);
    key(a, &ui, &t, VK_SPACE, 0); CHECK(a->cfg.start_with_windows == 0);
    // semilla con acentos (se guarda tal cual)
    click(a, &ui, &t, 280, 47); CHECK(ui.focus == ID_SEED);
    const uint32_t txt[] = {0xD1, 'a', 'n', 'd', 0xFA, ' ', '7'};
    for (int i = 0; i < 7; i++) ui_input_char(&ui, txt[i]);
    step(a, &ui, &t, 16);
    CHECK(!strcmp(a->cfg.seed, "\xC3\x91" "and\xC3\xBA 7"));
    // 'N' dentro del campo se escribe (no es atajo)
    int nn = a->n_new;
    ui_input_char(&ui, 'N'); ui_input_key(&ui, 'N', 1, 0, 0); step(a, &ui, &t, 16);
    CHECK(a->n_new == nn && !strcmp(a->cfg.seed, "\xC3\x91" "and\xC3\xBA 7N"));
    ui_input_key(&ui, VK_BACK, 1, 0, 0); step(a, &ui, &t, 16);
    // Ctrl+S aplica; el archivo tiene todo
    key(a, &ui, &t, 'S', UI_MOD_CTRL);
    CHECK(a->n_save == 2 && !a->dirty);
    fc_cfg_defaults(&f);
    CHECK(fc_cfg_load(&f, a->path) == 1);
    CHECK(!strcmp(f.seed, "\xC3\x91" "and\xC3\xBA 7") && f.speed == 400 && f.fps == 61 && fabs(f.zoom - 1.1) < 1e-6 &&
          f.pause_when_covered == 0 && f.start_with_windows == 0);
    CHECK(a->run_state == FC_RUN_ABSENT);   // simulado en dry-run

    // NUEVO PAISAJE con semilla fija: semilla nueva + aplicar
    char old[FC_SEED_BUF];
    strcpy(old, a->cfg.seed);
    click(a, &ui, &t, 144 + 42, 232 + 9);
    CHECK(strcmp(old, a->cfg.seed) != 0 && utf8_count(a->cfg.seed) == 8 && a->n_save == 3 && !a->dirty);
    fc_cfg_defaults(&f); fc_cfg_load(&f, a->path);
    CHECK(!strcmp(f.seed, a->cfg.seed));
    // OLVIDAR -> sucio; NUEVO PAISAJE en modo azar -> WM_COMMAND 3, no guarda
    click(a, &ui, &t, 380, 48);
    CHECK(a->cfg.seed[0] == 0 && a->dirty);
    nn = a->n_new;
    int ns = a->n_save;
    click(a, &ui, &t, 144 + 42, 232 + 9);
    CHECK(a->n_new == nn + 1 && a->n_save == ns && a->dirty);
    // COMO AL PRINCIPIO -> valores por defecto, falta aplicar
    click(a, &ui, &t, 340, 202);
    CHECK(a->cfg.speed == 20 && a->cfg.fps == 30 && a->cfg.start_with_windows == 1 && a->dirty);

    // tooltip de MARCHA: x = 224 - w, y = 66 (el alto depende de cuántas líneas salen con la fuente del toolkit)
    ui_input_mouse(&ui, 300, 66 + 16); step(a, &ui, &t, 16); step(a, &ui, &t, 500);
    CHECK(a->tip_id == ID_SPEED && a->tip_rect.y == 66 && a->tip_rect.x + a->tip_rect.w == 224 && (a->tip_rect.h - 30) % 9 == 0);
    fc_log("tooltip MARCHA: %d,%d,%d,%d", a->tip_rect.x, a->tip_rect.y, a->tip_rect.w, a->tip_rect.h);

    // RETIRARSE: mantener 900 ms -> destello 100 ms -> WM_COMMAND 1
    ui_input_mouse(&ui, 376, 242); step(a, &ui, &t, 16);
    ui_input_button(&ui, 1); step(a, &ui, &t, 16);
    step(a, &ui, &t, 400); CHECK(a->n_quit == 0 && ui.hold_id == ID_RETREAT);
    step(a, &ui, &t, 520); CHECK(a->quit_at != 0 && a->n_quit == 0);
    ui_input_button(&ui, 0);
    step(a, &ui, &t, 120); CHECK(a->n_quit == 1);
    // clic corto: no pasa nada, tooltip "mantené apretado"
    step(a, &ui, &t, POLL_MS);   // el sondeo (falso) vuelve a ver el fondo
    CHECK(a->retreat_enabled);
    nn = a->n_quit;
    click(a, &ui, &t, 376, 242);
    CHECK(a->n_quit == nn && a->tip_id == ID_RETREAT && !strcmp(a->tip_func, "MANTENÉ APRETADO PARA RETIRARTE."));
    step(a, &ui, &t, TAPTIP_MS + 50);

    // sin fondo: ENCENDER habilitado, RETIRARSE deshabilitado con su tooltip en 262,192,136,30
    a->fake_fondo = 0;
    step(a, &ui, &t, POLL_MS + 10);
    CHECK(!strcmp(a->main_label, "ENCENDER") && a->main_enabled && !a->retreat_enabled && !strcmp(a->st1, "NO HAY FONDO."));
    ui_input_mouse(&ui, 350, 240); step(a, &ui, &t, 16); step(a, &ui, &t, 500);
    // (el diseño da 262,192,136,30 con su fuente de 4 px; la del toolkit es más angosta: mismo y/h, x = 398 - w)
    CHECK(a->tip_id == ID_RETREAT && a->tip_rect.y == 192 && a->tip_rect.h == 30 && a->tip_rect.x + a->tip_rect.w == 398 &&
          a->tip_rect.w == font_text_width(FONT_SMALL, "NO HAY NADA QUE ABANDONAR.") + 10);
    fc_log("tooltip RETIRARSE deshabilitado: %d,%d,%d,%d", a->tip_rect.x, a->tip_rect.y, a->tip_rect.w, a->tip_rect.h);
    int nl = a->n_launch;
    click(a, &ui, &t, 232 + 29, 232 + 9);   // ENCENDER: guarda (estaba sucio) y lanza (dry)
    CHECK(a->n_launch == nl + 1 && !a->dirty && a->n_save == ns + 1);
    step(a, &ui, &t, SAVED_MS + 100);
    CHECK(!strcmp(a->main_label, "ENCENDER") && a->main_enabled);   // sigue sin fondo (dry-run)
    a->fake_fondo = 1;
    step(a, &ui, &t, POLL_MS + 10);
    CHECK(!strcmp(a->main_label, "APLICAR") && !a->main_enabled);

    // error al escribir: la ruta cuelga de un ARCHIVO -> NO PUDE GUARDAR, APLICAR sigue pendiente
    wchar_t keep[MAX_PATH], blocker[MAX_PATH];
    wcscpy(keep, a->path);
    wcscpy(blocker, a->path);
    wcscat(blocker, L".bloqueo");
    FILE *bf = _wfopen(blocker, L"wb");
    if (bf) fclose(bf);
    _snwprintf(a->path, MAX_PATH, L"%ls\\config.ini", blocker);
    ui_set_focus(&ui, ID_SPEED);
    key(a, &ui, &t, VK_LEFT, 0);
    click(a, &ui, &t, 232 + 29, 232 + 9);
    CHECK(a->error && a->dirty && a->main_enabled && !strcmp(a->st1, "NO PUDE GUARDAR."));
    wcscpy(a->path, keep);
    DeleteFileW(blocker);
    click(a, &ui, &t, 232 + 29, 232 + 9);
    CHECK(!a->error && !a->dirty && !strcmp(a->st1, "GUARDADO."));

    px_canvas_free(&c);
    int ra = fc_startup_query(FC_RUN_VALUE, NULL, real_after, 1024);
    CHECK(rb == ra && !wcscmp(real_before, real_after));   // la clave Run real no se tocó
    fc_log("contadores: guardar %d, recargar %d, lanzar %d, nuevo %d, salir %d (todos dry-run salvo el archivo temporal)",
           a->n_save, a->n_reload, a->n_launch, a->n_new, a->n_quit);
    fc_log(g_fail ? "SELFTEST: %d fallos" : "SELFTEST: OK", g_fail);
    return g_fail ? 1 : 0;
}

// ------------------------------------------------------------------ main
int main(void) {
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t *shot = NULL, *config = NULL;
    char state[32] = "anda";
    int scale = 3, quit_after = 0, test = 0, dry = 0, forcetip = 0;
    App *a = &g_app;
    a->fake_fondo = -1;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--shot") && i + 1 < argc) shot = argv[++i];
        else if (!wcscmp(argv[i], L"--config") && i + 1 < argc) config = argv[++i];
        else if (!wcscmp(argv[i], L"--scale") && i + 1 < argc) scale = _wtoi(argv[++i]);
        else if (!wcscmp(argv[i], L"--state") && i + 1 < argc) WideCharToMultiByte(CP_UTF8, 0, argv[++i], -1, state, sizeof state, NULL, NULL);
        else if (!wcscmp(argv[i], L"--fake-fondo") && i + 1 < argc) a->fake_fondo = _wtoi(argv[++i]) ? 1 : 0;
        else if (!wcscmp(argv[i], L"--quit-after") && i + 1 < argc) quit_after = _wtoi(argv[++i]);
        else if (!wcscmp(argv[i], L"--tip")) forcetip = 1;
        else if (!wcscmp(argv[i], L"--dry-run")) dry = 1;
        else if (!wcscmp(argv[i], L"--selftest")) test = 1;
    }
    if (config) { wcsncpy(a->path, config, MAX_PATH - 1); a->path[MAX_PATH - 1] = 0; }
    else fc_cfg_default_path(a->path, MAX_PATH);
    a->dry_sys = dry;
    a->dry_cfg = dry && !config;   // --dry-run sin --config: tampoco escribe el config.ini real
    if (!px_canvas_init(&a->scratch, CW, CH)) return 1;

    if (test) {
        if (!config) { fc_log("--selftest necesita --config <ruta temporal>"); return 2; }
        return selftest(a);
    }
    app_load(a);
    if (shot) return do_shot(a, shot, scale, state, forcetip);
    if (dry) fc_log("fondo-config: dry-run (registro y mensajes al fondo solo se describen%s)", a->dry_cfg ? "; config.ini tampoco se escribe" : "");

    PxWin w = {0};
    if (!pxwin_create(&w, "Fondo · Shan Shui", CW, CH, win_frame, NULL, a)) return 1;
    a->hwnd = w.hwnd;
    if (quit_after > 0) {   // prueba de humo: bombea mensajes un rato y cierra
        MSG msg;
        DWORD t0 = GetTickCount();
        while ((int)(GetTickCount() - t0) < quit_after && w.hwnd) {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            Sleep(10);
        }
        fc_log("quit-after: escala %d, cerrando", w.scale);
        pxwin_close(&w);
    }
    return pxwin_run(&w);
}
