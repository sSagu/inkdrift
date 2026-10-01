// ui: widgets pixel-art en modo inmediato (IMGUI) sobre un PxCanvas.
// Cada cuadro: ui_begin -> llamadas a widgets (dibujan y devuelven acciones) -> ui_end.
// Los eventos (mouse/teclado) se encolan con ui_input_* (o ui_win32_event) y se consumen
// en el siguiente cuadro. pxwin.c corre un cuadro por cada evento, asi que el orden se respeta.
#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "px.h"
#include "font.h"

// flags de widget
enum {
    UI_DISABLED = 1,   // gris, sin foco ni interaccion
    UI_PRIMARY  = 2,   // boton principal: borde amarillo (#dbce2d), p.ej. Aplicar con cambios
    UI_DANGER   = 4,   // relleno granate, hover naranja (Retirarse)
    UI_SMALLTXT = 8    // etiqueta con FONT_SMALL
};
// estados visuales (para ui_draw_* y para forzar estados en capturas)
enum { UI_ST_HOVER = 1, UI_ST_PRESSED = 2, UI_ST_FOCUS = 4, UI_ST_DISABLED = 8, UI_ST_ACTIVE = 16 };
// resultados (bitmask) de slider / campo de texto / hold
enum { UI_CHANGED = 1, UI_COMMIT = 2, UI_SUBMIT = 4, UI_HOLD_DONE = 8, UI_HOLD_TAP = 16 };
// modificadores de teclado
enum { UI_MOD_SHIFT = 1, UI_MOD_CTRL = 2, UI_MOD_ALT = 4 };

// Mapeo valor <-> posicion del slider. v = min + (max-min) * t^power (power 1 = lineal, 2 = 400*t^2).
// Si to_value/to_t no son NULL, reemplazan a la curva (deben ser inversas entre si).
typedef struct {
    double min, max;
    double power;          // 0 se toma como 1
    double step;           // paso de flechas y de redondeo (0 = continuo)
    double big_step;       // PgUp/PgDn (0 = 10*step)
    const double *ticks;   // muescas a dibujar (en unidades de valor), opcional
    int nticks;
    double (*to_value)(double t, void *user);
    double (*to_t)(double v, void *user);
    void *user;
} UiRange;
double ui_range_value(const UiRange *r, double t);
double ui_range_t(const UiRange *r, double v);

#define UI_TITLE_H 18      // alto de la barra de titulo propia (ui_titlebar)
#define UI_TIP_DELAY 400   // ms de hover antes del tooltip
#define UI_MAX_ITEMS 64
#define UI_MAX_EVENTS 32

typedef struct { int type, a, b, mods; } UiEvent;   // interno (ver ui_input_*)

typedef struct Ui {
    PxCanvas *c;
    uint32_t now;                 // ms (GetTickCount o simulado)
    // mouse (coordenadas logicas)
    int mx, my, mdown;
    // estado de interaccion
    int hot, active, focus;       // ids (0 = ninguno)
    int focus_visible;            // 1 tras navegar con teclado (muestra hormigas)
    int interactive;              // 0 = los widgets siguientes solo dibujan (modal abierto)
    // cola de eventos del cuadro
    UiEvent ev[UI_MAX_EVENTS]; int nev, evpos;
    int pressed, released;        // flancos del boton izquierdo en este cuadro
    // orden de foco del cuadro actual
    struct { int id; PxRect r; } items[UI_MAX_ITEMS]; int nitems;
    // tooltip
    int tip_id; uint32_t hot_since; int tip_forced; char tip[256];
    int prev_hot;
    // campo de texto con foco
    int tf_owner, tf_cursor, tf_scroll; uint32_t tf_blink0;   // tf_cursor en caracteres
    // hold
    uint32_t hold_t0; int hold_id, hold_key;
    // slider arrastrado
    int drag_id;
    // titulo propio: rectangulo arrastrable (para WM_NCHITTEST)
    PxRect caption;
    int anim;                     // 1 si algo se anima (el bucle debe seguir redibujando)
    int (*get_clipboard)(char *out, int outsz);   // por defecto: portapapeles de Windows
} Ui;

void ui_init(Ui *ui, PxCanvas *c);
void ui_begin(Ui *ui, uint32_t now_ms);
void ui_end(Ui *ui);                  // Tab/Shift+Tab, F1, tooltip encima de todo; vacia la cola
void ui_cancel(Ui *ui);               // perdida de foco/captura: suelta todo (cancela hold)

// ---- entrada (coordenadas ya en pixeles logicos)
void ui_input_mouse(Ui *ui, int x, int y);
void ui_input_button(Ui *ui, int down);
void ui_input_key(Ui *ui, int vk, int down, int repeat, int mods);   // vk = codigo VK_* de Windows
void ui_input_char(Ui *ui, uint32_t cp);                             // caracter Unicode ya armado
// Teclas no consumidas por widgets (para atajos de la app): 1 si se apreto vk con esos mods.
int ui_key_pressed(Ui *ui, int vk, int mods);
int ui_focus_is_text(const Ui *ui);
void ui_set_focus(Ui *ui, int id);
static inline void ui_set_interactive(Ui *ui, int on) { ui->interactive = on; }

// ---- widgets (id: entero unico y distinto de 0 elegido por la app)
int ui_button(Ui *ui, int id, PxRect r, const char *label, const PxIcon *icon, int flags);   // 1 = clic
int ui_hold_button(Ui *ui, int id, PxRect r, const char *label, const PxIcon *icon, int hold_ms, int flags);
int ui_toggle(Ui *ui, int id, PxRect r, const char *label, int *on, int flags);             // 1 = cambio
int ui_slider(Ui *ui, int id, PxRect r, double *value, const UiRange *rng, int flags);
int ui_textfield(Ui *ui, int id, PxRect r, char *buf, int bufsz, int maxchars, const char *placeholder, int flags);
void ui_tooltip(Ui *ui, int id, const char *text);      // llamar despues del widget
PxRect ui_panel(Ui *ui, PxRect r, const char *title, int style);   // devuelve el area interior
int ui_titlebar(Ui *ui, const char *title, int flags);  // barra de 16 px arriba; 1 = cerrar, 2 = minimizar
void ui_label(Ui *ui, int x, int y, const char *text, FontId f, uint32_t col, int txflags);

// ---- dibujo puro (sin interaccion): para galerias de estados y capturas
void ui_draw_button(PxCanvas *c, PxRect r, const char *label, const PxIcon *icon, int flags, int st, uint32_t now);
void ui_draw_hold(PxCanvas *c, PxRect r, const char *label, const PxIcon *icon, int flags, int st, double progress, uint32_t now);
void ui_draw_toggle(PxCanvas *c, PxRect r, const char *label, int on, int st, uint32_t now);
void ui_draw_slider(PxCanvas *c, PxRect r, double value, const UiRange *rng, int st, uint32_t now);
void ui_draw_textfield(PxCanvas *c, PxRect r, const char *buf, const char *placeholder, int st, int cursor, int scroll, int caret_on, uint32_t now);
void ui_draw_tooltip(PxCanvas *c, PxRect anchor, const char *text);

#ifdef _WIN32
#include <windows.h>
// Traduce un mensaje de Win32 a ui_input_*. scale = factor entero. Devuelve 1 si era de entrada.
int ui_win32_event(Ui *ui, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, int scale);
#endif

#endif
