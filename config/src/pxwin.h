// pxwin: ventana Win32 que muestra un PxCanvas escalado por un factor entero segun el DPI,
// con barra de titulo propia (WS_POPUP + HTCAPTION), doble buffer (DIB section) y un cuadro de
// UI por cada evento de entrada o tic del temporizador (33 ms mientras haya animacion).
#ifndef PXWIN_H
#define PXWIN_H
#include <windows.h>
#include "px.h"
#include "ui.h"

typedef struct PxWin PxWin;
typedef void (*PxFrameFn)(PxWin *w, void *user);   // dibuja la UI (entre ui_begin y ui_end)
typedef int (*PxCloseFn)(PxWin *w, void *user);    // WM_CLOSE: 1 = cerrar, 0 = no (p.ej. abrir modal)

struct PxWin {
    HWND hwnd;
    PxCanvas canvas;
    Ui ui;
    int scale;                 // factor entero actual
    int post;                  // PX_POST_* (scanlines / vineta)
    HBITMAP dib; uint32_t *bits; int dw, dh;
    PxFrameFn frame; PxCloseFn on_close; void *user;
};

// s = max(2, floor(2*dpi/96)); se baja si la ventana pasa el 85 % del area de trabajo (minimo 1).
int pxwin_pick_scale(int dpi, int work_w, int work_h, int cw, int ch);
// Crea y muestra la ventana centrada en el monitor del cursor. title en UTF-8.
int pxwin_create(PxWin *w, const char *title, int cw, int ch, PxFrameFn frame, PxCloseFn on_close, void *user);
void pxwin_redraw(PxWin *w);      // corre un cuadro y lo presenta
int pxwin_run(PxWin *w);          // bucle de mensajes; devuelve el codigo de salida
void pxwin_close(PxWin *w);       // destruye la ventana sin pasar por on_close

// Renderiza un cuadro sin ventana y lo guarda como PNG (GDI+) a la escala pedida.
int px_save_png(const wchar_t *path, const uint32_t *bgra, int w, int h);
int pxwin_shot(PxCanvas *c, int scale, int post, const wchar_t *path);

#endif
