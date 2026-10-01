// Papel de fondo y mezcla "multiply" (el mix-blend-mode:multiply del original).
#ifndef VIEW_H
#define VIEW_H
#include "core.h"

typedef struct { int size; uint8_t *rgb; } PaperTile;     // baldosa cuadrada, RGB

// Reescala la baldosa de papel de 512x512 (1 px = 1 px CSS) a la escala de pantalla.
PaperTile paper_scale(const uint8_t *src512, double kappa);

// dst (BGRA) = papel * arte / 255, para las filas [row0, row0+rows) de una imagen de ancho w.
// "art" es BGRA sobre blanco opaco (ya desplazado: la columna 0 de art es la columna 0 de dst).
void multiply_rows(uint8_t *dst, const uint8_t *art, int w, int row0, int rows, const PaperTile *p);

#endif
