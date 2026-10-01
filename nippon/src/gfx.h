// Dibujado: convierte los Items (polilineas con relleno y trazo) en pixeles con GDI+.
#ifndef GFX_H
#define GFX_H
#include "core.h"
#include "plan.h"

int gfx_init(void);
void gfx_shutdown(void);

// Dibuja todo el mundo sobre blanco opaco en un buffer BGRA (w*h*4).
// (x0,y0) = esquina superior izquierda en unidades del mundo, scale = pixeles por unidad.
void gfx_render(uint8_t *bgra, int w, int h, double x0, double y0, double scale);

// Guarda un buffer BGRA como PNG (uso: pruebas).
int gfx_save_png(const uint8_t *bgra, int w, int h, const wchar_t *path);

#endif
