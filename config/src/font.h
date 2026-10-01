// font: fuente bitmap propia (dibujada a mano), unicase en MAYUSCULAS, dos tamanos.
// Entrada UTF-8; se pasa a mayusculas (incluye a->A, n con tilde->N con tilde, etc.).
#ifndef FONT_H
#define FONT_H
#include <stdint.h>
#include "px.h"

typedef enum {
    FONT_SMALL = 0,   // altura de mayuscula 5 px, trazo de 1 px (cuerpo, tooltips, etiquetas)
    FONT_LARGE = 1    // altura de mayuscula 7 px, astas de 2 px (titulos, botones)
} FontId;

// Flags de dibujo
enum {
    TX_SHADOW = 1,    // sombra negra 1 px abajo-derecha (usar siempre salvo sobre pergamino)
    TX_CENTER = 2,    // x es el centro de la linea
    TX_RIGHT  = 4     // x es el borde derecho
};

// Metricas. "y" en todas las funciones de dibujo es la FILA SUPERIOR DE LAS MAYUSCULAS:
// los acentos se dibujan en las 3 filas de arriba (y-3..y-1) y las comas 1 fila por debajo.
int font_cap(FontId f);        // 5 o 7
int font_ascent(FontId f);     // filas reservadas arriba para acentos (3)
int font_line_h(FontId f);     // avance entre lineas ('\n'): 10 o 12

int font_text_width(FontId f, const char *utf8);    // ancho de la linea mas larga, sin sombra
// Dibuja (admite '\n'). Devuelve el ancho dibujado de la ultima linea.
int font_draw(PxCanvas *c, FontId f, int x, int y, const char *utf8, uint32_t col, int flags);
// Centra verticalmente las mayusculas dentro de r y alinea segun flags (izquierda por defecto).
void font_draw_in(PxCanvas *c, FontId f, PxRect r, const char *utf8, uint32_t col, int flags);
// Parte el texto en lineas de ancho <= maxw (por espacios). Escribe en out con '\n'.
// Devuelve la cantidad de lineas.
int font_wrap(FontId f, const char *utf8, int maxw, char *out, int outsz);
int font_has(FontId f, uint32_t cp);                // 1 si hay glifo (despues de pasar a mayuscula)

// ---- utilidades UTF-8
uint32_t utf8_next(const char **s);                 // decodifica y avanza (0 al final; U+FFFD si es invalido)
int utf8_put(uint32_t cp, char *out);               // codifica; devuelve bytes (1..4)
uint32_t utf8_upper(uint32_t cp);                   // a-z, a con tilde.., n con tilde, u con dieresis
int utf8_count(const char *s);                      // cantidad de caracteres (codepoints)
int utf8_offset(const char *s, int n);              // byte donde empieza el caracter n

#endif
