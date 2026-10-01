// Generadores del paisaje (port de las funciones de index.html).
// Convencion: los argumentos opcionales del original (objeto "args") son structs
// cuyos campos numericos valen UNDEF (NaN) si no se pasaron, y los de tipo
// funcion/color valen NULL / has_xxx=0. El valor por defecto se aplica adentro,
// en el mismo orden que el original (importa: algunos defaults consumen rnd()).
#ifndef GEN_H
#define GEN_H
#include "core.h"
#include "elements.h"

// ------------------------------------------------------------ primitivas ----
typedef struct {
    double xof, yof, wid, noi, out;
    Col col; int has_col;
    Fn fun; void *ctx;
} StrokeArgs;
StrokeArgs stroke_args(void);
void stroke(PL pts, const StrokeArgs *a);

// atajos usados muchisimo en el original
void stroke_c(PL pts, Col col, double wid);              // {col,wid}
double fn_one(double x, void *c);                         // function(x){return 1}
double fn_sin_pi(double x, void *c);                      // default de stroke

typedef struct {
    double len, wid, ang, noi, ret;
    Col col; int has_col;
    Fn fun; void *ctx;
} BlobArgs;
BlobArgs blob_args(void);
PL blob(double x, double y, const BlobArgs *a);          // si ret==0 dibuja y devuelve lista vacia

typedef double (*TexNoi)(double x, void *ctx);
typedef Col (*TexCol)(double x, void *ctx);
typedef double (*TexDis)(void *ctx);
typedef struct {
    double xof, yof, tex, wid, len, sha;
    TexNoi noi; TexCol col; TexDis dis; void *ctx;
} TexArgs;
TexArgs tex_args(void);
void texture(PLL ptlist, const TexArgs *a);

PLL triangulate(PL plist, double area, int convex, int optimize);

// ---------------------------------------------------------------- arboles ---
typedef struct {
    double hei, wid, noi, clu;
    Col col; int has_col;
    Fn ben; void *ctx;
} TreeArgs;
TreeArgs tree_args(void);
void tree01(double x, double y, const TreeArgs *a);
void tree02(double x, double y, const TreeArgs *a);
void tree03(double x, double y, const TreeArgs *a);
void tree04(double x, double y, const TreeArgs *a);
void tree05(double x, double y, const TreeArgs *a);
void tree06(double x, double y, const TreeArgs *a);
void tree07(double x, double y, const TreeArgs *a);
void tree08(double x, double y, const TreeArgs *a);

// ---------------------------------------------------------------- montanas --
typedef struct {
    double hei, wid, tex, veg, ret, cho, len, seg, sha;
    Col col; int has_col;
} MountArgs;
MountArgs mount_args(void);
void mountain(double xoff, double yoff, double seed, const MountArgs *a);
void flatMount(double xoff, double yoff, double seed, const MountArgs *a);
void distMount(double xoff, double yoff, double seed, const MountArgs *a);
void rock(double xoff, double yoff, double seed, const MountArgs *a);

// -------------------------------------------------------- arquitectura etc --
typedef struct {
    double hei, wid, rot, per, sto, sty, rai, len, sca, fli;
} ArchArgs;
ArchArgs arch_args(void);
void arch01(double xoff, double yoff, double seed, const ArchArgs *a);
void arch02(double xoff, double yoff, double seed, const ArchArgs *a);
void arch03(double xoff, double yoff, double seed, const ArchArgs *a);
void arch04(double xoff, double yoff, double seed, const ArchArgs *a);
void boat01(double xoff, double yoff, double seed, const ArchArgs *a);
void transmissionTower01(double xoff, double yoff, double seed, const ArchArgs *a);
void man(double xoff, double yoff, double sca, int hat, int ite, int fli, const double *len9);
void torii(double xoff, double yoff, double seed);          // Japon
void fuji(double xc, double yb, double seed, double hei, double len);   // Japon
// jp.c
void soft_tint(PL p, double xo, double yo, int r, int g, int b, double alpha, int layers);
void bamboo(double x, double y, double hei, int n);
void jpine(double x, double y, double hei);
void gravestone(double x, double y, double s);
void toro(double x, double y, double s);
void shrine_group(double x, double y);
void water_wash(double x, double y, double len);
void koi(double x, double y, double ang, double s);
void koi_pond(double x, double y);
void bridge(double x, double y, double len);
void samurai(double x, double y, double sca, int fli);
void duel(double x, double y, double sca);
void village(double x, double y);
void jboat(double x, double y, double sca, int fli);
void mecha(double x, double y, double h);
void stone_couple(double x, double y);
void islet(double x, double y, double w);
void kumo(double x, double y, double s);
void kasumi(double x, double y, double len);
void nami(double x, double y, double s);
void bridged_islets(double x, double y, double len);
void water(double xoff, double yoff, double seed, double hei, double len, double clu);

// ------------------------------------------------------------ planificador --
void gen_initial_chunks(FILE *out);

#endif
