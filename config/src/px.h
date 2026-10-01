// px: lienzo logico BGRA de baja resolucion + paleta cerrada + primitivas pixel-art.
// Sin dependencias de Win32 (solo C99), para poder probarlo y renderizarlo fuera de una ventana.
#ifndef PX_H
#define PX_H
#include <stdint.h>

// ---- Paleta cerrada (Lospec "Loop Hero" + #c0c2c2 para texto). Formato 0xAARRGGBB,
// que en memoria little-endian es B,G,R,A: el mismo orden que un DIB de 32 bits.
#define PX_BLACK   0xFF1d1a17u  // tinta: contornos
#define PX_SHADOW  0xFF2c3646u  // seda indigo (marco), ranuras
#define PX_MAROON  0xFF4a2e1fu  // madera oscura (rodillos), titulos
#define PX_DARK    0xFF5c4535u  // madera media (botones)
#define PX_RUST    0xFF86603fu  // veta clara de la madera
#define PX_MOSS    0xFF626439u  // oliva oscuro
#define PX_GRAY    0xFF8c8070u  // tinta diluida (textos secundarios)
#define PX_STEEL   0xFF497aa7u  // azul acero (rareza azul, cursor)
#define PX_ORANGE  0xFFc8553du  // bermellon vivo (hover de peligro)
#define PX_GREEN   0xFF6f8a4fu  // verde musgo (indicador)
#define PX_BRASS   0xFFb5432fu  // bermellon de sello (toggle, tirador, placa)
#define PX_TEAL    0xFF5ea2b0u  // celeste apagado
#define PX_PARCH   0xFFe9dfc8u  // papel de arroz
#define PX_FROST   0xFFa4bec1u  // gris azulado claro (foco)
#define PX_GOLD    0xFFd6ae5au  // oro suave (acento)
#define PX_WHITE   0xFFffffffu
#define PX_TEXT    0xFFf1e8d4u  // texto claro sobre madera/seda
#define PX_NONE    0x00000000u  // "transparente" (solo en iconos/argumentos: nunca se escribe)

typedef struct { int x, y, w, h; } PxRect;
static inline PxRect pxr(int x, int y, int w, int h) { PxRect r = {x, y, w, h}; return r; }
static inline int px_in(PxRect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
static inline PxRect px_inset(PxRect r, int d) { return pxr(r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d); }

typedef struct {
    int w, h;
    uint32_t *px;          // w*h pixeles, fila 0 arriba
    PxRect clip;           // todo dibujo se recorta a este rectangulo
} PxCanvas;

int  px_canvas_init(PxCanvas *c, int w, int h);   // 1 si ok (reserva memoria)
void px_canvas_free(PxCanvas *c);
PxRect px_set_clip(PxCanvas *c, PxRect r);        // devuelve el clip anterior (r se intersecta con el lienzo)
void px_reset_clip(PxCanvas *c);

// ---- primitivas basicas
void px_clear(PxCanvas *c, uint32_t col);
void px_pset(PxCanvas *c, int x, int y, uint32_t col);
uint32_t px_get(const PxCanvas *c, int x, int y);
void px_fill(PxCanvas *c, PxRect r, uint32_t col);
void px_hline(PxCanvas *c, int x, int y, int w, uint32_t col);
void px_vline(PxCanvas *c, int x, int y, int h, uint32_t col);
void px_line(PxCanvas *c, int x0, int y0, int x1, int y1, uint32_t col);   // Bresenham
void px_outline(PxCanvas *c, PxRect r, uint32_t col);                       // contorno 1 px por dentro de r

// Octogono: rectangulo con esquinas cortadas en escalera de 45 grados (k px por esquina).
void px_chamfer_fill(PxCanvas *c, PxRect r, int k, uint32_t col);
void px_chamfer_outline(PxCanvas *c, PxRect r, int k, uint32_t col);       // borde 1 px del octogono
// Bisel de luz arriba-izquierda: 1 px por dentro del contorno (k = chaflan del contorno, 0 si es recto).
void px_bevel(PxCanvas *c, PxRect r, int k, uint32_t light);
void px_bevel_dark(PxCanvas *c, PxRect r, int k, uint32_t dark);          // 1 px abajo-derecha por dentro

// ---- tramados (dither) y texturas, siempre en colores de paleta
enum { PX_DITHER_50 = 0, PX_DITHER_25 = 1, PX_DITHER_75 = 2, PX_DITHER_12 = 3 };
void px_dither(PxCanvas *c, PxRect r, uint32_t col, int pattern, int phase);
// Oscurece un area al ~50 % con tablero negro (fondo de un modal).
static inline void px_dim(PxCanvas *c, PxRect r) { px_dither(c, r, PX_BLACK, PX_DITHER_50, 0); }
// Motas deterministicas: ~permil/1000 de los pixeles de r (hash de x,y,seed: siempre el mismo dibujo).
void px_noise(PxCanvas *c, PxRect r, uint32_t seed, int permil, uint32_t col);
// Grietas: n caminatas aleatorias deterministicas de 4..10 px.
void px_cracks(PxCanvas *c, PxRect r, uint32_t seed, int n, uint32_t col);
uint32_t px_hash(uint32_t x, uint32_t y, uint32_t seed);

// Color de paleta mas oscuro "vecino" (deshabilitado = todo un escalon mas oscuro).
uint32_t px_darker(uint32_t col);

// ---- paneles (receta de loophero-visual.md seccion 8)
enum { PX_PANEL_GRAY, PX_PANEL_DARK, PX_PANEL_MAROON, PX_PANEL_BLACK, PX_PANEL_PARCH, PX_PANEL_SLOT };
void px_panel(PxCanvas *c, PxRect r, int style, uint32_t seed);

// Separador: linea de 1 px con rombo 5x5 en el centro (y = fila de la linea).
void px_separator(PxCanvas *c, int x, int y, int w, uint32_t col);
// Rombo relleno de "radio" rad (3x3 con rad=1, 5x5 con rad=2) centrado en (cx,cy).
void px_diamond(PxCanvas *c, int cx, int cy, int rad, uint32_t col);

// Contorno de foco "hormigas": tablero blanco/#a4bec1 a 1 px por fuera de r, siguiendo el chaflan k.
// phase cambia cada ~150 ms para animarlo (0 = quieto).
void px_focus_ants(PxCanvas *c, PxRect r, int k, int phase);
// Anillo de progreso: recorre el perimetro del octogono r (y 1 px por fuera) en sentido horario
// desde arriba al centro; pinta la fraccion t (0..1) con col.
void px_ring_progress(PxCanvas *c, PxRect r, int k, double t, uint32_t col);

// ---- iconos: mapa de caracteres a colores, con contorno negro automatico.
//   '.' transparente; 'k' negro; 'd' #232323; 'g' #686f6f; 'p' #9fa089; 'w' blanco; 't' #c0c2c2;
//   'f' #a4bec1; 'b' #af9156; 'y' #dbce2d; 'r' #602217; 'o' #cd6627; 's' #497aa7; 'u' #815938;
//   'v' #879b42; 'c' #5ea2b0; 'm' #626439; 'D' #3a3f3f
typedef struct { int w, h; const char *rows; } PxIcon;   // rows: h filas de w chars separadas por ' '
enum { PX_ICON_OUTLINE = 1, PX_ICON_MONO = 2 };          // MONO: todo pixel no transparente = col
void px_icon(PxCanvas *c, int x, int y, const PxIcon *ic, int flags, uint32_t mono_col);
uint32_t px_char_color(char ch);

// Iconos incluidos (dibujados a mano, sin assets del juego). Medidas sin contorno.
extern const PxIcon PX_ICON_DIE;      // 7x7 dado (Tirar semilla)
extern const PxIcon PX_ICON_SKULL;    // 7x7 calavera (grupo Costumbres)
extern const PxIcon PX_ICON_RUN;      // 7x9 hombre corriendo (Retirarse)
extern const PxIcon PX_ICON_MOUNTAIN; // 11x7 montanas (Nuevo paisaje)
extern const PxIcon PX_ICON_FIRE;     // 7x8 fogata
extern const PxIcon PX_ICON_CLOSE;    // 5x5 cruz (cerrar / Olvidar)
extern const PxIcon PX_ICON_MIN;      // 5x5 guion bajo (minimizar)

// ---- escalado y post-proceso (sobre la imagen ya escalada, unica excepcion a "sin gradientes")
enum { PX_POST_SCANLINES = 1, PX_POST_VIGNETTE = 2 };
// dst: (c->w*scale) x (c->h*scale) pixeles, fila 0 arriba. Vecino mas cercano, factor entero.
void px_upscale(const PxCanvas *c, uint32_t *dst, int scale, int post);

#endif
