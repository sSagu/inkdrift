// ui: widgets en modo inmediato (ver ui.h).
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#else
enum { VK_BACK = 8, VK_TAB = 9, VK_RETURN = 13, VK_ESCAPE = 27, VK_SPACE = 32, VK_PRIOR = 33, VK_NEXT = 34,
       VK_END = 35, VK_HOME = 36, VK_LEFT = 37, VK_UP = 38, VK_RIGHT = 39, VK_DOWN = 40, VK_DELETE = 46, VK_F1 = 112 };
#endif

enum { EV_NONE, EV_KEYDOWN, EV_KEYUP, EV_CHAR };

static int sys_clipboard(char *out, int outsz);

void ui_init(Ui *ui, PxCanvas *c) {
    memset(ui, 0, sizeof *ui);
    ui->c = c;
    ui->mx = ui->my = -10000;
    ui->interactive = 1;
    ui->get_clipboard = sys_clipboard;
}

void ui_begin(Ui *ui, uint32_t now) {
    ui->now = now;
    ui->hot = 0;
    ui->nitems = 0;
    ui->tip_id = 0;
    ui->anim = 0;
    ui->interactive = 1;
    ui->caption = pxr(0, 0, 0, 0);
}

void ui_cancel(Ui *ui) {
    ui->active = 0;
    ui->mdown = 0;
    ui->hold_id = 0;
    ui->hold_key = 0;
    ui->drag_id = 0;
}

void ui_input_mouse(Ui *ui, int x, int y) { ui->mx = x; ui->my = y; }
void ui_input_button(Ui *ui, int down) {
    if (down && !ui->mdown) ui->pressed = 1;
    if (!down && ui->mdown) ui->released = 1;
    ui->mdown = down;
}
static void push(Ui *ui, int type, int a, int b, int mods) {
    if (ui->nev >= UI_MAX_EVENTS) return;
    UiEvent e = {type, a, b, mods};
    ui->ev[ui->nev++] = e;
}
void ui_input_key(Ui *ui, int vk, int down, int repeat, int mods) { push(ui, down ? EV_KEYDOWN : EV_KEYUP, vk, repeat, mods); }
void ui_input_char(Ui *ui, uint32_t cp) { push(ui, EV_CHAR, (int)cp, 0, 0); }

// Busca un evento de tecla no consumido; si lo encuentra lo consume.
static int take_key(Ui *ui, int type, int vk, int mods_mask, int mods, int allow_repeat) {
    for (int i = 0; i < ui->nev; i++) {
        UiEvent *e = &ui->ev[i];
        if (e->type == type && e->a == vk && (e->mods & mods_mask) == mods && (allow_repeat || !e->b)) {
            e->type = EV_NONE;
            return 1;
        }
    }
    return 0;
}
int ui_key_pressed(Ui *ui, int vk, int mods) { return take_key(ui, EV_KEYDOWN, vk, UI_MOD_SHIFT | UI_MOD_CTRL | UI_MOD_ALT, mods, 0); }

int ui_focus_is_text(const Ui *ui) { return ui->focus && ui->focus == ui->tf_owner; }
void ui_set_focus(Ui *ui, int id) { ui->focus = id; }

// Registra el widget: orden de Tab, hot y clic de mouse. Devuelve 1 si el mouse esta adentro.
static int item(Ui *ui, int id, PxRect r, int flags) {
    int en = ui->interactive && !(flags & UI_DISABLED);
    if (en && ui->nitems < UI_MAX_ITEMS) { ui->items[ui->nitems].id = id; ui->items[ui->nitems].r = r; ui->nitems++; }
    int inside = px_in(r, ui->mx, ui->my);
    if (!en) { if (ui->active == id) ui->active = 0; return 0; }
    if (inside && (ui->active == 0 || ui->active == id)) ui->hot = id;
    if (ui->pressed && inside && ui->active == 0) {
        ui->active = id;
        ui->focus = id;
        ui->focus_visible = 0;
    }
    return inside;
}

static int state(Ui *ui, int id, int flags, int inside) {
    if ((flags & UI_DISABLED) || !ui->interactive) return (flags & UI_DISABLED) ? UI_ST_DISABLED : 0;
    int st = 0;
    if (ui->hot == id) st |= UI_ST_HOVER;
    if (ui->active == id && inside) st |= UI_ST_PRESSED;
    if (ui->focus == id && ui->focus_visible) st |= UI_ST_FOCUS;
    return st;
}

static int ants_phase(uint32_t now) { return (int)(now / 150) & 1; }

// ------------------------------------------------------------------ dibujo
static void content(PxCanvas *c, PxRect r, const char *label, const PxIcon *icon, int flags, uint32_t col, int dy) {
    FontId f = (flags & UI_SMALLTXT) ? FONT_SMALL : FONT_LARGE;
    int tw = label && *label ? font_text_width(f, label) : 0;
    int iw = icon ? icon->w : 0, gap = (iw && tw) ? 4 : 0;
    int x = r.x + (r.w - (iw + gap + tw)) / 2;
    if (icon) {
        int mono = (col == PX_GRAY) ? PX_ICON_MONO : 0;
        px_icon(c, x, r.y + (r.h - icon->h) / 2 + dy, icon, PX_ICON_OUTLINE | mono, PX_DARK);
    }
    if (tw) font_draw(c, f, x + iw + gap, r.y + (r.h - font_cap(f)) / 2 + dy, label, col, TX_SHADOW);
}

void ui_draw_button(PxCanvas *c, PxRect r, const char *label, const PxIcon *icon, int flags, int st, uint32_t now) {
    int k = 2, dis = st & UI_ST_DISABLED, prs = (st & UI_ST_PRESSED) && !dis, hov = (st & UI_ST_HOVER) && !dis;
    uint32_t fill, light, text;
    if (dis) { fill = PX_SHADOW; light = PX_NONE; text = PX_GRAY; }
    else if (flags & UI_DANGER) { fill = hov ? PX_ORANGE : PX_MAROON; light = hov ? PX_GOLD : PX_RUST; text = hov ? PX_WHITE : PX_TEXT; }
    else { fill = hov ? PX_PARCH : PX_DARK; light = hov ? PX_FROST : PX_PARCH; text = hov ? PX_WHITE : PX_TEXT; }
    if (prs) { fill = (flags & UI_DANGER) ? PX_RUST : PX_GRAY; light = PX_NONE; }
    px_chamfer_fill(c, r, k, fill);
    px_chamfer_outline(c, r, k, ((flags & UI_PRIMARY) && !dis) ? PX_GOLD : PX_BLACK);
    if (light) px_bevel(c, r, k, light);
    if (!prs && !dis) px_bevel_dark(c, r, k, PX_SHADOW);
    content(c, r, label, icon, flags, text, prs ? 1 : 0);
    if (st & UI_ST_FOCUS) px_focus_ants(c, r, k, ants_phase(now));
}

void ui_draw_hold(PxCanvas *c, PxRect r, const char *label, const PxIcon *icon, int flags, int st, double p, uint32_t now) {
    ui_draw_button(c, r, label, icon, flags, st, now);
    if (p > 0 && !(st & UI_ST_DISABLED)) px_ring_progress(c, r, 2, p, PX_GOLD);
}

void ui_draw_toggle(PxCanvas *c, PxRect r, const char *label, int on, int st, uint32_t now) {
    int dis = st & UI_ST_DISABLED, hov = (st & UI_ST_HOVER) && !dis;
    PxRect b = pxr(r.x, r.y + (r.h - 9) / 2, 9, 9);
    px_fill(c, b, PX_SHADOW);
    px_outline(c, b, hov ? PX_PARCH : PX_BLACK);
    if (on) {
        PxRect in = px_inset(b, 2);
        px_fill(c, in, dis ? PX_GRAY : PX_BRASS);
        if (!dis) { px_hline(c, in.x, in.y, in.w, PX_GOLD); px_vline(c, in.x, in.y, in.h, PX_GOLD); }
        px_hline(c, in.x + 1, in.y + in.h, in.w, PX_BLACK);
    } else {
        px_hline(c, b.x + 1, b.y + 1, 7, PX_BLACK);
    }
    if (label) font_draw(c, FONT_SMALL, b.x + 13, r.y + (r.h - 5) / 2, label, dis ? PX_GRAY : hov ? PX_WHITE : PX_TEXT, TX_SHADOW);
    if (st & UI_ST_FOCUS) {
        int w = label ? 13 + font_text_width(FONT_SMALL, label) + 2 : 9;
        px_focus_ants(c, pxr(b.x - 1, b.y - 1, w + 2, 11), 0, ants_phase(now));
    }
}

double ui_range_value(const UiRange *g, double t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    if (g->to_value) return g->to_value(t, g->user);
    double p = g->power > 0 ? g->power : 1;
    return g->min + (g->max - g->min) * pow(t, p);
}
double ui_range_t(const UiRange *g, double v) {
    if (g->to_t) return g->to_t(v, g->user);
    if (g->max == g->min) return 0;
    double u = (v - g->min) / (g->max - g->min);
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    double p = g->power > 0 ? g->power : 1;
    return pow(u, 1.0 / p);
}
static double snap(const UiRange *g, double v) {
    if (g->step > 0) v = g->min + floor((v - g->min) / g->step + 0.5) * g->step;
    if (v < g->min) v = g->min;
    if (v > g->max) v = g->max;
    return v;
}

enum { KNOB_W = 5, KNOB_H = 9 };

void ui_draw_slider(PxCanvas *c, PxRect r, double v, const UiRange *g, int st, uint32_t now) {
    int dis = st & UI_ST_DISABLED, cy = r.y + r.h / 2;
    int span = r.w - KNOB_W, kx = r.x + (int)floor(ui_range_t(g, v) * span + 0.5);
    PxRect tr = pxr(r.x, cy - 1, r.w, 3);
    px_fill(c, tr, PX_BLACK);
    px_hline(c, r.x + 1, cy, r.w - 2, PX_SHADOW);
    px_hline(c, r.x + 1, cy, kx - r.x, dis ? PX_DARK : PX_STEEL);
    for (int i = 0; i < g->nticks; i++) {
        int tx = r.x + KNOB_W / 2 + (int)floor(ui_range_t(g, g->ticks[i]) * span + 0.5);
        px_vline(c, tx, cy + 3, 2, dis ? PX_DARK : (tx <= kx + 2 ? PX_PARCH : PX_GRAY));
    }
    PxRect kb = pxr(kx, cy - KNOB_H / 2 - 1, KNOB_W, KNOB_H);
    uint32_t fill = dis ? PX_GRAY : (st & UI_ST_ACTIVE) ? PX_GOLD : PX_BRASS;
    px_chamfer_fill(c, kb, 1, fill);
    px_chamfer_outline(c, kb, 1, PX_BLACK);
    if (!dis) px_bevel(c, kb, 1, (st & UI_ST_HOVER) ? PX_WHITE : PX_GOLD);
    if (!dis) px_bevel_dark(c, kb, 1, PX_RUST);
    if (st & UI_ST_FOCUS) px_focus_ants(c, kb, 1, ants_phase(now));
}

void ui_draw_textfield(PxCanvas *c, PxRect r, const char *buf, const char *ph, int st, int cursor, int scroll, int caret_on, uint32_t now) {
    px_panel(c, r, PX_PANEL_SLOT, 0);
    if ((st & UI_ST_ACTIVE) || (st & UI_ST_HOVER)) px_outline(c, r, (st & UI_ST_ACTIVE) ? PX_PARCH : PX_GRAY);
    PxRect in = pxr(r.x + 3, r.y + 1, r.w - 6, r.h - 2);
    PxRect old = px_set_clip(c, in);
    int ty = r.y + (r.h - 5) / 2 + 1;
    if (!buf[0] && ph) font_draw(c, FONT_SMALL, in.x, ty, ph, PX_GRAY, 0);
    else font_draw(c, FONT_SMALL, in.x - scroll, ty, buf, (st & UI_ST_DISABLED) ? PX_GRAY : PX_TEXT, TX_SHADOW);
    if ((st & UI_ST_ACTIVE) && caret_on) {
        char tmp[512];
        int off = utf8_offset(buf, cursor);
        if (off > (int)sizeof tmp - 1) off = sizeof tmp - 1;
        memcpy(tmp, buf, (size_t)off); tmp[off] = 0;
        int cx = in.x - scroll + (off ? font_text_width(FONT_SMALL, tmp) + 1 : 0);
        px_vline(c, cx, ty - 1, 7, PX_WHITE);
    }
    c->clip = old;
    if (st & UI_ST_FOCUS) px_focus_ants(c, r, 0, ants_phase(now));
}

void ui_draw_tooltip(PxCanvas *c, PxRect a, const char *text) {
    char buf[512];
    int lines = font_wrap(FONT_SMALL, text, 140, buf, sizeof buf);
    int w = font_text_width(FONT_SMALL, buf) + 9, h = 14 + (lines - 1) * font_line_h(FONT_SMALL) + 2;
    int x = a.x + a.w + 4, y = a.y;
    if (x + w > c->w - 2) x = a.x - w - 4;
    if (x < 2) { x = a.x; y = a.y + a.h + 3; if (x + w > c->w - 2) x = c->w - 2 - w; }
    if (y + h > c->h - 2) y = c->h - 2 - h;
    if (y < 2) y = 2;
    px_panel(c, pxr(x, y, w, h), PX_PANEL_BLACK, 0);
    font_draw(c, FONT_SMALL, x + 4, y + 6, buf, PX_TEXT, TX_SHADOW);
}

// ------------------------------------------------------------------ widgets
static int key_activate(Ui *ui, int id) {
    if (ui->focus != id) return 0;
    return take_key(ui, EV_KEYDOWN, VK_SPACE, UI_MOD_CTRL | UI_MOD_ALT, 0, 0) ||
           take_key(ui, EV_KEYDOWN, VK_RETURN, UI_MOD_CTRL | UI_MOD_ALT, 0, 0);
}

int ui_button(Ui *ui, int id, PxRect r, const char *label, const PxIcon *icon, int flags) {
    int inside = item(ui, id, r, flags), clicked = 0;
    if (ui->interactive && !(flags & UI_DISABLED)) {
        if (ui->released && ui->active == id) { clicked = inside; ui->active = 0; }
        if (key_activate(ui, id)) clicked = 1;
    }
    int st = state(ui, id, flags, inside);
    if (st & UI_ST_FOCUS) ui->anim = 1;
    ui_draw_button(ui->c, r, label, icon, flags, st, ui->now);
    return clicked;
}

int ui_hold_button(Ui *ui, int id, PxRect r, const char *label, const PxIcon *icon, int hold_ms, int flags) {
    int inside = item(ui, id, r, flags), res = 0;
    double p = 0;
    if (ui->interactive && !(flags & UI_DISABLED)) {
        if (ui->pressed && ui->active == id) { ui->hold_id = id; ui->hold_t0 = ui->now; ui->hold_key = 0; }
        if (ui->focus == id && ui->hold_id != id) {
            if (take_key(ui, EV_KEYDOWN, VK_SPACE, UI_MOD_CTRL | UI_MOD_ALT, 0, 0)) { ui->hold_id = id; ui->hold_t0 = ui->now; ui->hold_key = VK_SPACE; }
            else if (take_key(ui, EV_KEYDOWN, VK_RETURN, UI_MOD_CTRL | UI_MOD_ALT, 0, 0)) { ui->hold_id = id; ui->hold_t0 = ui->now; ui->hold_key = VK_RETURN; }
        }
        if (ui->hold_id == id) {
            if (ui->hold_key) take_key(ui, EV_KEYDOWN, ui->hold_key, 0, 0, 1);   // traga la auto-repeticion
            int released = ui->hold_key ? take_key(ui, EV_KEYUP, ui->hold_key, 0, 0, 1) || ui->focus != id
                                        : (ui->released || ui->active != id);
            if (!ui->hold_key && !inside) ui->hold_t0 = ui->now;   // fuera del boton: el anillo vuelve a 0
            p = (double)(ui->now - ui->hold_t0) / (hold_ms > 0 ? hold_ms : 1000);
            if (p >= 1) { res = UI_HOLD_DONE; ui->hold_id = 0; ui->hold_key = 0; if (ui->active == id) ui->active = 0; p = 0; }
            else if (released) { res = UI_HOLD_TAP; ui->hold_id = 0; ui->hold_key = 0; p = 0; }
            else ui->anim = 1;
        }
        if (ui->released && ui->active == id) ui->active = 0;
    }
    int st = state(ui, id, flags, inside);
    if (ui->hold_id == id) st |= UI_ST_PRESSED;
    if (st & UI_ST_FOCUS) ui->anim = 1;
    ui_draw_hold(ui->c, r, label, icon, flags, st, p, ui->now);
    return res;
}

int ui_toggle(Ui *ui, int id, PxRect r, const char *label, int *on, int flags) {
    int inside = item(ui, id, r, flags), ch = 0;
    if (ui->interactive && !(flags & UI_DISABLED)) {
        if (ui->released && ui->active == id) { ch = inside; ui->active = 0; }
        if (key_activate(ui, id)) ch = 1;
        if (ui->focus == id && (take_key(ui, EV_KEYDOWN, VK_LEFT, 7, 0, 1) || take_key(ui, EV_KEYDOWN, VK_RIGHT, 7, 0, 1))) ch = 1;
        if (ch) *on = !*on;
    }
    int st = state(ui, id, flags, inside);
    if (st & UI_ST_FOCUS) ui->anim = 1;
    ui_draw_toggle(ui->c, r, label, *on, st, ui->now);
    return ch;
}

int ui_slider(Ui *ui, int id, PxRect r, double *value, const UiRange *g, int flags) {
    PxRect hit = pxr(r.x, r.y + r.h / 2 - 6, r.w, 13);
    int inside = item(ui, id, hit, flags), res = 0;
    double v = *value;
    if (ui->interactive && !(flags & UI_DISABLED)) {
        if (ui->active == id && (ui->mdown || ui->released)) {
            double t = (double)(ui->mx - r.x - KNOB_W / 2) / (r.w - KNOB_W);
            v = snap(g, ui_range_value(g, t));
            ui->drag_id = id;
        }
        if (ui->released && ui->active == id) { ui->active = 0; ui->drag_id = 0; res |= UI_COMMIT; }
        if (ui->focus == id) {
            double big = g->big_step > 0 ? g->big_step : (g->step > 0 ? 10 * g->step : (g->max - g->min) / 10);
            double st = g->step > 0 ? g->step : (g->max - g->min) / 100;
            int kb = 0;
            while (take_key(ui, EV_KEYDOWN, VK_LEFT, 7, 0, 1) || take_key(ui, EV_KEYDOWN, VK_DOWN, 7, 0, 1)) { v -= st; kb = 1; }
            while (take_key(ui, EV_KEYDOWN, VK_RIGHT, 7, 0, 1) || take_key(ui, EV_KEYDOWN, VK_UP, 7, 0, 1)) { v += st; kb = 1; }
            while (take_key(ui, EV_KEYDOWN, VK_PRIOR, 7, 0, 1)) { v += big; kb = 1; }
            while (take_key(ui, EV_KEYDOWN, VK_NEXT, 7, 0, 1)) { v -= big; kb = 1; }
            if (take_key(ui, EV_KEYDOWN, VK_HOME, 7, 0, 1)) { v = g->min; kb = 1; }
            if (take_key(ui, EV_KEYDOWN, VK_END, 7, 0, 1)) { v = g->max; kb = 1; }
            if (kb) { v = snap(g, v); res |= UI_COMMIT; ui->focus_visible = 1; }
            if (take_key(ui, EV_KEYDOWN, VK_RETURN, 7, 0, 0)) res |= UI_SUBMIT;
        }
    }
    if (v != *value) { *value = v; res |= UI_CHANGED; }
    int st = state(ui, id, flags, inside);
    if (ui->active == id) st |= UI_ST_ACTIVE;
    if (st & UI_ST_FOCUS) ui->anim = 1;
    ui_draw_slider(ui->c, r, *value, g, st, ui->now);
    return res;
}

static int text_px(const char *buf, int nchars) {
    char tmp[512];
    int off = utf8_offset(buf, nchars);
    if (off <= 0) return 0;
    if (off > (int)sizeof tmp - 1) off = sizeof tmp - 1;
    memcpy(tmp, buf, (size_t)off); tmp[off] = 0;
    return font_text_width(FONT_SMALL, tmp) + 1;
}

// Inserta el texto s (UTF-8) en la posicion del cursor respetando maxchars y bufsz.
static int tf_insert(Ui *ui, char *buf, int bufsz, int maxchars, const char *s) {
    int n = utf8_count(buf), ins = 0;
    while (*s && n < maxchars) {
        const char *q = s;
        uint32_t cp = utf8_next(&q);
        if (cp < 32 || cp == 127) { s = q; continue; }   // sin saltos de linea ni control
        int len = (int)(q - s), off = utf8_offset(buf, ui->tf_cursor), bl = (int)strlen(buf);
        if (bl + len >= bufsz) break;
        memmove(buf + off + len, buf + off, (size_t)(bl - off + 1));
        memcpy(buf + off, s, (size_t)len);
        ui->tf_cursor++; n++; ins = 1;
        s = q;
    }
    return ins;
}

int ui_textfield(Ui *ui, int id, PxRect r, char *buf, int bufsz, int maxchars, const char *ph, int flags) {
    int inside = item(ui, id, r, flags), res = 0;
    if (ui->interactive && !(flags & UI_DISABLED)) {
        if (ui->focus == id && ui->tf_owner != id) {   // foco recien ganado
            ui->tf_owner = id;
            ui->tf_cursor = utf8_count(buf);
            ui->tf_scroll = 0;
            ui->tf_blink0 = ui->now;
        }
        if (ui->pressed && ui->active == id) {   // clic: cursor al caracter mas cercano
            int n = utf8_count(buf), best = n, bd = 1 << 30;
            for (int i = 0; i <= n; i++) {
                int d = abs(r.x + 3 - ui->tf_scroll + text_px(buf, i) - ui->mx);
                if (d < bd) { bd = d; best = i; }
            }
            ui->tf_cursor = best;
            ui->tf_blink0 = ui->now;
        }
        if (ui->released && ui->active == id) ui->active = 0;
        if (ui->focus == id) {
            int n = utf8_count(buf), moved = 0;
            if (ui->tf_cursor > n) ui->tf_cursor = n;
            for (int i = 0; i < ui->nev; i++) {
                UiEvent *e = &ui->ev[i];
                if (e->type == EV_CHAR) {
                    char tmp[8]; int l = utf8_put((uint32_t)e->a, tmp); tmp[l] = 0;
                    if (tf_insert(ui, buf, bufsz, maxchars, tmp)) res |= UI_CHANGED;
                    e->type = EV_NONE; moved = 1;
                } else if (e->type == EV_KEYDOWN) {
                    int off = utf8_offset(buf, ui->tf_cursor), used = 1;
                    switch (e->a) {
                    case VK_LEFT: if (ui->tf_cursor > 0) ui->tf_cursor--; break;
                    case VK_RIGHT: if (ui->tf_cursor < utf8_count(buf)) ui->tf_cursor++; break;
                    case VK_HOME: ui->tf_cursor = 0; break;
                    case VK_END: ui->tf_cursor = utf8_count(buf); break;
                    case VK_BACK:
                        if (ui->tf_cursor > 0) {
                            int p = utf8_offset(buf, ui->tf_cursor - 1);
                            memmove(buf + p, buf + off, strlen(buf + off) + 1);
                            ui->tf_cursor--; res |= UI_CHANGED;
                        }
                        break;
                    case VK_DELETE:
                        if (buf[off]) {
                            int q = utf8_offset(buf, ui->tf_cursor + 1);
                            memmove(buf + off, buf + q, strlen(buf + q) + 1);
                            res |= UI_CHANGED;
                        }
                        break;
                    case 'V':
                        if (e->mods & UI_MOD_CTRL) {
                            char clip[1024];
                            if (ui->get_clipboard && ui->get_clipboard(clip, sizeof clip) && tf_insert(ui, buf, bufsz, maxchars, clip)) res |= UI_CHANGED;
                        } else used = 0;
                        break;
                    case VK_RETURN: res |= UI_SUBMIT; break;
                    default: used = 0;
                    }
                    if (used) { e->type = EV_NONE; moved = 1; }
                }
            }
            if (moved) ui->tf_blink0 = ui->now;
            // desplazamiento horizontal para que el cursor quede a la vista
            int cx = text_px(buf, ui->tf_cursor), vis = r.w - 7;
            if (cx - ui->tf_scroll > vis) ui->tf_scroll = cx - vis;
            if (cx - ui->tf_scroll < 0) ui->tf_scroll = cx;
            if (ui->tf_scroll < 0) ui->tf_scroll = 0;
            ui->anim = 1;
        }
    }
    int st = state(ui, id, flags, inside);
    int editing = ui->focus == id && ui->interactive && !(flags & UI_DISABLED);
    if (editing) st |= UI_ST_ACTIVE;
    int caret = editing && ((ui->now - ui->tf_blink0) / 530) % 2 == 0;
    ui_draw_textfield(ui->c, r, buf, ph, st, editing ? ui->tf_cursor : 0, editing ? ui->tf_scroll : 0, caret, ui->now);
    return res;
}

void ui_tooltip(Ui *ui, int id, const char *text) {
    if (!text || !*text) return;
    int hover = id == ui->prev_hot && ui->hot == id && !ui->mdown && ui->now - ui->hot_since >= UI_TIP_DELAY;
    int forced = ui->tip_forced && ui->focus == id;
    if (id == ui->hot && !hover) ui->anim = 1;   // esperando el retardo
    if (hover || forced) {
        ui->tip_id = id;
        strncpy(ui->tip, text, sizeof ui->tip - 1);
        ui->tip[sizeof ui->tip - 1] = 0;
    }
}

PxRect ui_panel(Ui *ui, PxRect r, const char *title, int style) {
    px_panel(ui->c, r, style, (uint32_t)(r.x * 31 + r.y * 7 + r.w));
    if (!title) return px_inset(r, 3);
    font_draw(ui->c, FONT_LARGE, r.x + r.w / 2, r.y + 6, title, PX_TEXT, TX_SHADOW | TX_CENTER);
    px_separator(ui->c, r.x + 4, r.y + 17, r.w - 8, PX_PARCH);
    return pxr(r.x + 3, r.y + 21, r.w - 6, r.h - 24);
}

int ui_titlebar(Ui *ui, const char *title, int flags) {
    PxCanvas *c = ui->c;
    PxRect bar = pxr(0, 0, c->w, UI_TITLE_H);
    px_fill(c, bar, PX_DARK);
    px_noise(c, px_inset(bar, 2), 99, 15, PX_SHADOW);
    px_outline(c, bar, PX_BLACK);
    px_bevel(c, bar, 0, PX_GRAY);
    font_draw(c, FONT_LARGE, 7, (UI_TITLE_H - 7) / 2, title, PX_TEXT, TX_SHADOW);
    int res = 0, bw = 15, bh = 12, by = (UI_TITLE_H - bh) / 2;
    PxRect bc = pxr(c->w - bw - 3, by, bw, bh), bm = pxr(c->w - 2 * bw - 5, by, bw, bh);
    int save = ui->interactive;
    if (ui_button(ui, 0x7FFF0001, bc, NULL, &PX_ICON_CLOSE, UI_DANGER)) res = 1;
    if (!(flags & 1) && ui_button(ui, 0x7FFF0002, bm, NULL, &PX_ICON_MIN, 0)) res = 2;
    ui->interactive = save;
    // los botones del titulo no entran en el orden de Tab
    for (int i = 0; i < ui->nitems; i++)
        if (ui->items[i].id >= 0x7FFF0001) { memmove(&ui->items[i], &ui->items[i + 1], sizeof ui->items[0] * (size_t)(ui->nitems - i - 1)); ui->nitems--; i--; }
    ui->caption = pxr(0, 0, ((flags & 1) ? bc.x : bm.x) - 2, UI_TITLE_H);
    return res;
}

void ui_label(Ui *ui, int x, int y, const char *text, FontId f, uint32_t col, int txflags) {
    font_draw(ui->c, f, x, y, text, col, txflags);
}

void ui_end(Ui *ui) {
    // el foco desaparecio (widget oculto o deshabilitado)
    int found = 0;
    for (int i = 0; i < ui->nitems; i++) if (ui->items[i].id == ui->focus) found = 1;
    if (!found) ui->focus = 0;
    // Tab / Shift+Tab
    for (int i = 0; i < ui->nev; i++) {
        UiEvent *e = &ui->ev[i];
        if (e->type != EV_KEYDOWN || e->a != VK_TAB || ui->nitems == 0) continue;
        int cur = -1;
        for (int j = 0; j < ui->nitems; j++) if (ui->items[j].id == ui->focus) cur = j;
        int back = e->mods & UI_MOD_SHIFT;
        int nx = cur < 0 ? (back ? ui->nitems - 1 : 0) : (cur + (back ? ui->nitems - 1 : 1)) % ui->nitems;
        ui->focus = ui->items[nx].id;
        ui->focus_visible = 1;
        ui->tip_forced = 0;
        e->type = EV_NONE;
    }
    if (take_key(ui, EV_KEYDOWN, VK_F1, 7, 0, 0)) ui->tip_forced = !ui->tip_forced;
    if (ui->focus != ui->tf_owner) ui->tf_owner = 0;
    if (ui->released) ui->active = 0;
    // tooltip arriba de todo
    if (ui->tip_id) {
        for (int i = 0; i < ui->nitems; i++)
            if (ui->items[i].id == ui->tip_id) { ui_draw_tooltip(ui->c, ui->items[i].r, ui->tip); break; }
    }
    if (ui->hot != ui->prev_hot) { ui->prev_hot = ui->hot; ui->hot_since = ui->now; }
    ui->pressed = ui->released = 0;
    ui->nev = 0;
}

// ------------------------------------------------------------------ Win32
#ifdef _WIN32
static int sys_clipboard(char *out, int outsz) {
    int ok = 0;
    out[0] = 0;
    if (!OpenClipboard(NULL)) return 0;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t *w = (const wchar_t *)GlobalLock(h);
        if (w) { ok = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, outsz, NULL, NULL) > 0; GlobalUnlock(h); }
    }
    CloseClipboard();
    if (!ok) out[outsz - 1] = 0;
    return ok;
}

static int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

int ui_win32_event(Ui *ui, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, int scale) {
    static wchar_t hi_sur;
    int mods = (GetKeyState(VK_SHIFT) < 0 ? UI_MOD_SHIFT : 0) | (GetKeyState(VK_CONTROL) < 0 ? UI_MOD_CTRL : 0) |
               (GetKeyState(VK_MENU) < 0 ? UI_MOD_ALT : 0);
    switch (msg) {
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT t = {sizeof t, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&t);
        ui_input_mouse(ui, fdiv(GET_X_LPARAM(lp), scale), fdiv(GET_Y_LPARAM(lp), scale));
        return 1;
    }
    case WM_MOUSELEAVE: if (!ui->mdown) ui_input_mouse(ui, -10000, -10000); return 1;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        SetCapture(hwnd);
        ui_input_mouse(ui, fdiv(GET_X_LPARAM(lp), scale), fdiv(GET_Y_LPARAM(lp), scale));
        ui_input_button(ui, 1);
        return 1;
    case WM_LBUTTONUP:
        ui_input_mouse(ui, fdiv(GET_X_LPARAM(lp), scale), fdiv(GET_Y_LPARAM(lp), scale));
        ui_input_button(ui, 0);
        ReleaseCapture();
        return 1;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        ui_input_key(ui, (int)wp, 1, (int)((lp >> 30) & 1), mods);
        return 1;
    case WM_KEYUP: case WM_SYSKEYUP:
        ui_input_key(ui, (int)wp, 0, 0, mods);
        return 1;
    case WM_CHAR: {
        wchar_t w = (wchar_t)wp;
        if (w >= 0xD800 && w < 0xDC00) { hi_sur = w; return 0; }
        uint32_t cp = w;
        if (w >= 0xDC00 && w < 0xE000) { if (!hi_sur) return 0; cp = 0x10000 + (((uint32_t)hi_sur - 0xD800) << 10) + (w - 0xDC00); }
        hi_sur = 0;
        if (cp >= 32 && cp != 127) { ui_input_char(ui, cp); return 1; }
        return 0;
    }
    case WM_KILLFOCUS: ui_cancel(ui); return 1;
    case WM_CAPTURECHANGED: if ((HWND)lp != hwnd) { if (ui->mdown) ui->released = 1; ui->mdown = 0; ui->hold_id = 0; } return 1;
    }
    return 0;
}
#else
static int sys_clipboard(char *out, int outsz) { (void)outsz; out[0] = 0; return 0; }
#endif
