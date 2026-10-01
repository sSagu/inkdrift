// Nucleo del port de shan-shui-inf a C.
// Todo lo que dice "JS:" replica el comportamiento exacto del original, porque
// el objetivo es que, con la misma semilla, salga el mismo paisaje que la web.
#ifndef CORE_H
#define CORE_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------- arena ----
// Memoria temporal de la generacion: se reserva rapido y se libera de golpe.
void *ar_alloc(size_t n);
size_t ar_mark(void);
void ar_reset(size_t mark);

// ----------------------------------------------------------------- PRNG ----
// Replica Prng del original (s = s*s % m, en doble precision).
void prng_seed_str(const char *seed);   // Math.seed(SEED)
double rnd(void);                        // Math.random()
extern double g_prng_s;                  // estado, para comparar con el oraculo

// ---------------------------------------------------------------- Noise ----
// Ruido Perlin de p5.js (3D), con la tabla llenada de forma perezosa con rnd().
double noise3(double x, double y, double z);
void noise_reset(void);                  // vuelve a llenar la tabla en la proxima llamada
#define NOISE1(x) noise3((x), 0, 0)
#define NOISE2(x, y) noise3((x), (y), 0)

// ----------------------------------------------------------- puntos/listas --
typedef struct { double x, y; } Pt;
typedef struct { Pt *p; int n, cap; } PL;      // lista de puntos (array JS de [x,y])
typedef struct { PL *l; int n, cap; } PLL;     // lista de listas

PL pl_new(void);
void pl_push(PL *a, double x, double y);
PL pl_copy(PL a);
PL pl_cat(PL a, PL b);                  // a.concat(b)
void pl_reverse(PL *a);                 // a.reverse() (in place)
PL pl_slice(PL a, int from, int to);    // a.slice(from,to); to<0 cuenta desde el final
void pl_splice(PL *a, int start, int count);   // a.splice(start,count)
void pl_unshift(PL *a, double x, double y);
PL pl_off(PL a, double xo, double yo);  // a.map(v => [v[0]+xo, v[1]+yo])
PL pl_lit(int n, const double *xy);     // lista literal [[x,y],...]

PLL pll_new(void);
void pll_push(PLL *a, PL l);

// ---------------------------------------------------------------- colores --
typedef struct { uint8_t r, g, b; uint8_t kind; double a; } Col; // kind: 0 rgba, 1 none, 2 white, 3 rgb(r,g,b)
extern const Col COL_NONE, COL_WHITE;
Col rgba(int r, int g, int b, double a);          // rgba(r,g,b,a) sin redondeo
double toFixedN(double v, int digits);            // Number(v.toFixed(digits))
Col rgbaq(int r, int g, int b, double a, int digits);  // alpha = toFixed(digits)
int col_is_rgba(Col c);
Col rgb3(int r, int g, int b);                    // "rgb(r,g,b)" (opaco)

// ----------------------------------------------------- lista de dibujo -----
// Todo lo que se dibuja pasa por poly(): se guarda como "Item".
typedef struct {
    uint8_t type;       // 0 polilinea, 1 texto
    Col fil, str;
    float wid;
    int n;              // cantidad de puntos
    int32_t *pts;       // x,y en decimas de unidad (toFixed(1) como el original)
    int32_t x0, y0, x1, y1;   // caja contenedora (con el trazo), en decimas
    // texto
    float fsize;
    float angle;
    char *text;
} Item;

typedef struct {
    char tag[16];
    double x, y;
    Item *items;
    int n, cap;
} Chunk;

extern Chunk *g_cur;                      // donde caen los poly() actuales
void chunk_init(Chunk *c, const char *tag, double x, double y);
void chunk_free(Chunk *c);
void chunk_append(Chunk *dst, Chunk *src);   // mueve los items de src al final de dst
void chunk_dump(FILE *f, const Chunk *c); // formato canonico para el oraculo
void chunk_dump_items(FILE *f, const Chunk *c);   // igual, sin la linea "C|..."
void emit_text(double x, double y, double fsize, double angle_deg, Col fill, const char *text);

// poly(plist, {xof,yof,fil,str,wid})
void poly(PL pts, double xof, double yof, Col fil, Col str, double wid);
// Elementos apagados: se generan igual (misma secuencia aleatoria, el resto del paisaje no
// cambia) pero mientras g_mute > 0 poly()/emit_text() no dibujan nada.
extern int g_mute;
extern unsigned g_el_off;                        // bit i = elemento i apagado (ver elements.h)
#define EL_ON(id) (!(g_el_off & (1u << (id))))
#define EL(id, ...) do { int _m = !EL_ON(id); g_mute += _m; __VA_ARGS__; g_mute -= _m; } while (0)
// atajo: str = fil (el valor por defecto del original)
void poly_f(PL pts, double xof, double yof, Col fil, double wid);

// ----------------------------------------------------------------- utiles --
typedef double (*Fn)(double x, void *ctx);

#define UNDEF NAN
#define ISU(v) isnan(v)
#define ARG(v, def) (isnan(v) ? (def) : (v))

double distance(Pt a, Pt b);
double mapval(double v, double i0, double i1, double o0, double o1);
void loopNoise(double *ns, int n);
double randChoiceD(const double *arr, int n);   // randChoice sobre numeros
int randChoiceI(const int *arr, int n);
double normRand(double m, double M);
double wtrand(double (*f)(double));
double randGaussian(void);
Pt midPt(const Pt *p, int n);
PL div_pl(PL a, double reso);
PL bezmh(PL P, double w);
int jsfloor_i(double v);

#endif
