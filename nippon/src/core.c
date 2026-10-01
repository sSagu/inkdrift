#include "core.h"
#include <windows.h>

// ---------------------------------------------------------------- arena ----
static uint8_t *g_ar_base;
static size_t g_ar_used, g_ar_commit;
#define AR_RESERVE ((size_t)1 << 30)   // 1 GB de direcciones, se confirma a demanda
#define AR_STEP ((size_t)4 << 20)

void *ar_alloc(size_t n) {
    if (!g_ar_base) {
        g_ar_base = VirtualAlloc(NULL, AR_RESERVE, MEM_RESERVE, PAGE_READWRITE);
        if (!g_ar_base) { fputs("sin memoria (arena)\n", stderr); exit(1); }
    }
    n = (n + 15) & ~(size_t)15;
    if (g_ar_used + n > AR_RESERVE) { fputs("arena agotada\n", stderr); exit(1); }
    while (g_ar_used + n > g_ar_commit) {
        if (!VirtualAlloc(g_ar_base + g_ar_commit, AR_STEP, MEM_COMMIT, PAGE_READWRITE)) {
            fputs("sin memoria (commit)\n", stderr); exit(1);
        }
        g_ar_commit += AR_STEP;
    }
    void *p = g_ar_base + g_ar_used;
    g_ar_used += n;
    return p;
}
size_t ar_mark(void) { return g_ar_used; }
void ar_reset(size_t mark) {
    g_ar_used = mark;
    // devolver al sistema lo que sobre de 64 MB confirmados
    size_t keep = (mark + AR_STEP - 1) / AR_STEP * AR_STEP;
    if (keep < ((size_t)64 << 20)) keep = (size_t)64 << 20;
    if (g_ar_commit > keep) {
        VirtualFree(g_ar_base + keep, g_ar_commit - keep, MEM_DECOMMIT);
        g_ar_commit = keep;
    }
}

// ----------------------------------------------------------------- PRNG ----
double g_prng_s = 1234;
static const double PRNG_P = 999979, PRNG_Q = 999983;
static double prng_m(void) { return PRNG_P * PRNG_Q; }

double rnd(void) {
    g_prng_s = fmod(g_prng_s * g_prng_s, prng_m());
    return g_prng_s / prng_m();
}

// btoa(JSON.stringify(seed)) y la suma ponderada del original (en doble precision).
static double prng_hash(const char *seed) {
    char json[512];
    int k = 0;
    json[k++] = '"';
    for (const char *s = seed; *s && k < 500; s++) {
        if (*s == '"' || *s == '\\') json[k++] = '\\';
        json[k++] = *s;
    }
    json[k++] = '"';
    json[k] = 0;

    static const char *B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char y[800];
    int yn = 0;
    for (int i = 0; i < k; i += 3) {
        uint32_t v = (uint8_t)json[i] << 16;
        int have = 1;
        if (i + 1 < k) { v |= (uint8_t)json[i + 1] << 8; have = 2; }
        if (i + 2 < k) { v |= (uint8_t)json[i + 2]; have = 3; }
        y[yn++] = B64[(v >> 18) & 63];
        y[yn++] = B64[(v >> 12) & 63];
        y[yn++] = have >= 2 ? B64[(v >> 6) & 63] : '=';
        y[yn++] = have >= 3 ? B64[v & 63] : '=';
    }
    double z = 0, pw = 1;
    for (int i = 0; i < yn; i++) {
        z += (double)(uint8_t)y[i] * pw;   // charCodeAt(i) * Math.pow(128, i)
        pw *= 128;
    }
    return z;
}

void prng_seed_str(const char *seed) {
    double m = prng_m();
    double h = prng_hash(seed);
    double y = 0, z = 0;
    while (fmod(y, PRNG_P) == 0 || fmod(y, PRNG_Q) == 0 || y == 0 || y == 1) {
        y = fmod(h + z, m);
        z += 1;
    }
    g_prng_s = y;
    for (int i = 0; i < 10; i++) rnd();
}

// ---------------------------------------------------------------- Noise ----
enum { PERLIN_MASK = 4095, YWRAPB = 4, YWRAP = 1 << 4, ZWRAPB = 8, ZWRAP = 1 << 8, OCTAVES = 4 };
static double g_perlin[PERLIN_MASK + 1];
static int g_perlin_ready;

static int32_t toi32(double v) {   // ToInt32 de JS
    if (!isfinite(v)) return 0;
    v = trunc(v);
    v = fmod(v, 4294967296.0);
    if (v < 0) v += 4294967296.0;
    return (int32_t)(uint32_t)v;
}
void noise_reset(void) { g_perlin_ready = 0; }
static double scaled_cosine(double i) { return 0.5 * (1.0 - cos(i * M_PI)); }

double noise3(double x, double y, double z) {
    if (isnan(y)) y = 0;     // y = y || 0
    if (isnan(z)) z = 0;
    if (!g_perlin_ready) {
        for (int i = 0; i < PERLIN_MASK + 1; i++) g_perlin[i] = rnd();
        g_perlin_ready = 1;
    }
    if (x < 0) x = -x;
    if (y < 0) y = -y;
    if (z < 0) z = -z;

    double xi = floor(x), yi = floor(y), zi = floor(z);
    double xf = x - xi, yf = y - yi, zf = z - zi;
    double r = 0, ampl = 0.5;

    for (int o = 0; o < OCTAVES; o++) {
        double of = xi + (double)(int32_t)((uint32_t)toi32(yi) << YWRAPB)
                       + (double)(int32_t)((uint32_t)toi32(zi) << ZWRAPB);
        double rxf = scaled_cosine(xf), ryf = scaled_cosine(yf);
        double n1, n2, n3;

        n1 = g_perlin[toi32(of) & PERLIN_MASK];
        n1 += rxf * (g_perlin[toi32(of + 1) & PERLIN_MASK] - n1);
        n2 = g_perlin[toi32(of + YWRAP) & PERLIN_MASK];
        n2 += rxf * (g_perlin[toi32(of + YWRAP + 1) & PERLIN_MASK] - n2);
        n1 += ryf * (n2 - n1);

        of += ZWRAP;
        n2 = g_perlin[toi32(of) & PERLIN_MASK];
        n2 += rxf * (g_perlin[toi32(of + 1) & PERLIN_MASK] - n2);
        n3 = g_perlin[toi32(of + YWRAP) & PERLIN_MASK];
        n3 += rxf * (g_perlin[toi32(of + YWRAP + 1) & PERLIN_MASK] - n3);
        n2 += ryf * (n3 - n2);

        n1 += scaled_cosine(zf) * (n2 - n1);

        r += n1 * ampl;
        ampl *= 0.5;
        xi = (double)(int32_t)((uint32_t)toi32(xi) << 1); xf *= 2;
        yi = (double)(int32_t)((uint32_t)toi32(yi) << 1); yf *= 2;
        zi = (double)(int32_t)((uint32_t)toi32(zi) << 1); zf *= 2;
        if (xf >= 1.0) { xi++; xf--; }
        if (yf >= 1.0) { yi++; yf--; }
        if (zf >= 1.0) { zi++; zf--; }
    }
    return r;
}

// ----------------------------------------------------------- puntos/listas --
PL pl_new(void) { PL a = {0, 0, 0}; return a; }

void pl_push(PL *a, double x, double y) {
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap * 2 : 16;
        Pt *np = ar_alloc((size_t)nc * sizeof(Pt));
        if (a->n) memcpy(np, a->p, (size_t)a->n * sizeof(Pt));
        a->p = np;
        a->cap = nc;
    }
    a->p[a->n].x = x;
    a->p[a->n].y = y;
    a->n++;
}
PL pl_copy(PL a) {
    PL b = pl_new();
    if (a.n) {
        b.p = ar_alloc((size_t)a.n * sizeof(Pt));
        memcpy(b.p, a.p, (size_t)a.n * sizeof(Pt));
        b.n = b.cap = a.n;
    }
    return b;
}
PL pl_cat(PL a, PL b) {
    PL c = pl_new();
    int n = a.n + b.n;
    if (n) {
        c.p = ar_alloc((size_t)n * sizeof(Pt));
        if (a.n) memcpy(c.p, a.p, (size_t)a.n * sizeof(Pt));
        if (b.n) memcpy(c.p + a.n, b.p, (size_t)b.n * sizeof(Pt));
        c.n = c.cap = n;
    }
    return c;
}
void pl_reverse(PL *a) {
    for (int i = 0, j = a->n - 1; i < j; i++, j--) {
        Pt t = a->p[i]; a->p[i] = a->p[j]; a->p[j] = t;
    }
}
PL pl_slice(PL a, int from, int to) {
    int n = a.n;
    if (from < 0) from = n + from < 0 ? 0 : n + from;
    if (to < 0) to = n + to < 0 ? 0 : n + to;
    if (from > n) from = n;
    if (to > n) to = n;
    PL c = pl_new();
    if (to > from) {
        c.p = ar_alloc((size_t)(to - from) * sizeof(Pt));
        memcpy(c.p, a.p + from, (size_t)(to - from) * sizeof(Pt));
        c.n = c.cap = to - from;
    }
    return c;
}
void pl_splice(PL *a, int start, int count) {
    if (start > a->n) start = a->n;
    if (start + count > a->n) count = a->n - start;
    if (count <= 0) return;
    memmove(a->p + start, a->p + start + count, (size_t)(a->n - start - count) * sizeof(Pt));
    a->n -= count;
}
void pl_unshift(PL *a, double x, double y) {
    pl_push(a, 0, 0);
    memmove(a->p + 1, a->p, (size_t)(a->n - 1) * sizeof(Pt));
    a->p[0].x = x;
    a->p[0].y = y;
}
PL pl_off(PL a, double xo, double yo) {
    PL c = pl_new();
    if (a.n) {
        c.p = ar_alloc((size_t)a.n * sizeof(Pt));
        for (int i = 0; i < a.n; i++) { c.p[i].x = a.p[i].x + xo; c.p[i].y = a.p[i].y + yo; }
        c.n = c.cap = a.n;
    }
    return c;
}
PL pl_lit(int n, const double *xy) {
    PL c = pl_new();
    for (int i = 0; i < n; i++) pl_push(&c, xy[2 * i], xy[2 * i + 1]);
    return c;
}

PLL pll_new(void) { PLL a = {0, 0, 0}; return a; }
void pll_push(PLL *a, PL l) {
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap * 2 : 8;
        PL *np = ar_alloc((size_t)nc * sizeof(PL));
        if (a->n) memcpy(np, a->l, (size_t)a->n * sizeof(PL));
        a->l = np;
        a->cap = nc;
    }
    a->l[a->n++] = l;
}

// ---------------------------------------------------------------- colores --
const Col COL_NONE = {0, 0, 0, 1, 0};
const Col COL_WHITE = {255, 255, 255, 2, 1};

Col rgba(int r, int g, int b, double a) {
    Col c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 0, (float)a};
    return c;
}
double toFixedN(double v, int digits) {
    double s = 1;
    for (int i = 0; i < digits; i++) s *= 10;
    double sign = v < 0 ? -1 : 1;
    return sign * floor(fabs(v) * s + 0.5) / s;
}
Col rgbaq(int r, int g, int b, double a, int digits) { return rgba(r, g, b, toFixedN(a, digits)); }
int col_is_rgba(Col c) { return c.kind == 0; }
Col rgb3(int r, int g, int b) { Col c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 3, 1.0}; return c; }

// ----------------------------------------------------- lista de dibujo -----
Chunk *g_cur;

void chunk_init(Chunk *c, const char *tag, double x, double y) {
    memset(c, 0, sizeof *c);
    strncpy(c->tag, tag, sizeof c->tag - 1);
    c->x = x;
    c->y = y;
}
void chunk_free(Chunk *c) {
    for (int i = 0; i < c->n; i++) { free(c->items[i].pts); free(c->items[i].text); }
    free(c->items);
    c->items = NULL;
    c->n = c->cap = 0;
}
static Item *chunk_add(Chunk *c);
void chunk_append(Chunk *dst, Chunk *src) {
    for (int i = 0; i < src->n; i++) { Item *it = chunk_add(dst); *it = src->items[i]; }
    free(src->items);
    src->items = NULL;
    src->n = src->cap = 0;
}
static Item *chunk_add(Chunk *c) {
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 256;
        c->items = realloc(c->items, (size_t)c->cap * sizeof(Item));
        if (!c->items) { fputs("sin memoria (items)\n", stderr); exit(1); }
    }
    Item *it = &c->items[c->n++];
    memset(it, 0, sizeof *it);
    return it;
}

static int32_t tenths(double v) {      // (v).toFixed(1) -> decimas enteras; NaN -> -1000
    if (isnan(v)) return -10000;
    double s = v < 0 ? -1 : 1;
    double t = s * floor(fabs(v) * 10 + 0.5);
    if (t > 2e9) t = 2e9;
    if (t < -2e9) t = -2e9;
    return (int32_t)t;
}

int g_skip_invisible = 0;
int g_mute = 0;
unsigned g_el_off = 0;

void poly(PL pts, double xof, double yof, Col fil, Col str, double wid) {
    if (g_mute) return;
    int vis_fill = fil.kind != 1 && fil.a > 0;
    int vis_str = str.kind != 1 && str.a > 0 && wid > 0;
    if (g_skip_invisible && !vis_fill && !vis_str) return;
    Item *it = chunk_add(g_cur);
    it->type = 0;
    it->fil = fil;
    it->str = str;
    it->wid = (float)wid;
    it->n = pts.n;
    it->pts = malloc((size_t)(pts.n ? pts.n : 1) * 2 * sizeof(int32_t));
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = 0; i < pts.n; i++) {
        int32_t x = tenths(pts.p[i].x + xof), y = tenths(pts.p[i].y + yof);
        it->pts[2 * i] = x;
        it->pts[2 * i + 1] = y;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    int32_t pad = vis_str ? (int32_t)(wid * 20) + 20 : 10;   // miter limit 4 => hasta 2*ancho
    if (pts.n == 0) { x0 = y0 = x1 = y1 = 0; }
    it->x0 = x0 - pad; it->y0 = y0 - pad; it->x1 = x1 + pad; it->y1 = y1 + pad;
}
void poly_f(PL pts, double xof, double yof, Col fil, double wid) { poly(pts, xof, yof, fil, fil, wid); }

void emit_text(double x, double y, double fsize, double angle_deg, Col fill, const char *text) {
    if (g_mute) return;
    Item *it = chunk_add(g_cur);
    it->type = 1;
    it->fil = fill;
    it->fsize = (float)fsize;
    it->angle = (float)angle_deg;
    it->text = _strdup(text);
    int32_t cx = tenths(x), cy = tenths(y);
    int32_t r = (int32_t)(fsize * 10 * 6);
    it->n = 1;
    it->pts = malloc(2 * sizeof(int32_t));
    it->pts[0] = cx; it->pts[1] = cy;
    it->x0 = cx - r; it->x1 = cx + r; it->y0 = cy - r; it->y1 = cy + r;
}

static void print_col(FILE *f, Col c) {
    if (c.kind == 1) fputs("none", f);
    else if (c.kind == 2) fputs("white", f);
    else if (c.kind == 3) fprintf(f, "rgb(%d,%d,%d)", c.r, c.g, c.b);
    else fprintf(f, "rgba(%d,%d,%d,%.4g)", c.r, c.g, c.b, (double)c.a);
}

void chunk_dump(FILE *f, const Chunk *c) {
    fprintf(f, "C|%s|%.17g|%.17g\n", c->tag, c->x, c->y);
    chunk_dump_items(f, c);
}

void chunk_dump_items(FILE *f, const Chunk *c) {
    for (int i = 0; i < c->n; i++) {
        const Item *it = &c->items[i];
        if (it->type == 1) {
            fprintf(f, "T|%.4g|", (double)it->fsize);
            print_col(f, it->fil);
            fprintf(f, "|%.1f|%.1f|%.3f|%s\n", it->pts[0] / 10.0, it->pts[1] / 10.0, (double)it->angle, it->text);
            continue;
        }
        fputs("P|", f);
        print_col(f, it->fil);
        fputc('|', f);
        print_col(f, it->str);
        fprintf(f, "|%.6g|", (double)it->wid);
        for (int k = 0; k < it->n; k++) {
            if (k) fputc(';', f);
            fprintf(f, "%.1f,%.1f", it->pts[2 * k] / 10.0, it->pts[2 * k + 1] / 10.0);
        }
        fputc('\n', f);
    }
}

// ----------------------------------------------------------------- utiles --
double distance(Pt a, Pt b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);   // Math.pow(x,2) == x*x
}
double mapval(double v, double i0, double i1, double o0, double o1) {
    return o0 + (o1 - o0) * (((v - i0) * 1.0) / (i1 - i0));
}
void loopNoise(double *ns, int n) {
    double dif = ns[n - 1] - ns[0];
    double b0 = 100, b1 = -100;
    for (int i = 0; i < n; i++) {
        ns[i] += (dif * (n - 1 - i)) / (n - 1);
        if (ns[i] < b0) b0 = ns[i];
        if (ns[i] > b1) b1 = ns[i];
    }
    for (int i = 0; i < n; i++) ns[i] = mapval(ns[i], b0, b1, 0, 1);
}
double randChoiceD(const double *arr, int n) { return arr[(int)floor(n * rnd())]; }
int randChoiceI(const int *arr, int n) { return arr[(int)floor(n * rnd())]; }
double normRand(double m, double M) { return mapval(rnd(), 0, 1, m, M); }
double wtrand(double (*f)(double)) {
    for (;;) {
        double x = rnd();
        double y = rnd();
        if (y < f(x)) return x;
    }
}
static double gauss_f(double x) { double d = x - 0.5; return pow(M_E, -24 * (d * d)); }
double randGaussian(void) { return wtrand(gauss_f) * 2 - 1; }

Pt midPt(const Pt *p, int n) {
    Pt acc = {0, 0};
    for (int i = 0; i < n; i++) {
        acc.x = p[i].x / n + acc.x;
        acc.y = p[i].y / n + acc.y;
    }
    return acc;
}

PL div_pl(PL a, double reso) {
    double tl = (a.n - 1) * reso;
    PL r = pl_new();
    for (double i = 0; i < tl; i += 1) {
        Pt lastp = a.p[(int)floor(i / reso)];
        Pt nextp = a.p[(int)ceil(i / reso)];
        double p = fmod(i, reso) / reso;
        double nx = lastp.x * (1 - p) + nextp.x * p;
        double ny = lastp.y * (1 - p) + nextp.y * p;
        pl_push(&r, nx, ny);
    }
    if (a.n > 0) pl_push(&r, a.p[a.n - 1].x, a.p[a.n - 1].y);
    return r;
}

PL bezmh(PL P, double w) {
    if (P.n == 2) {
        Pt two[2] = {P.p[0], P.p[1]};
        Pt m = midPt(two, 2);
        PL q = pl_new();
        pl_push(&q, P.p[0].x, P.p[0].y);
        pl_push(&q, m.x, m.y);
        pl_push(&q, P.p[1].x, P.p[1].y);
        P = q;
    }
    PL plist = pl_new();
    for (int j = 0; j < P.n - 2; j++) {
        Pt p0, p1, p2;
        if (j == 0) p0 = P.p[j];
        else { Pt t[2] = {P.p[j], P.p[j + 1]}; p0 = midPt(t, 2); }
        p1 = P.p[j + 1];
        if (j == P.n - 3) p2 = P.p[j + 2];
        else { Pt t[2] = {P.p[j + 1], P.p[j + 2]}; p2 = midPt(t, 2); }
        int pl = 20;
        int lim = pl + (j == P.n - 3 ? 1 : 0);
        for (int i = 0; i < lim; i += 1) {
            double t = (double)i / pl;
            double a = 1 - t;
            double u = a * a + 2 * t * a * w + t * t;
            pl_push(&plist,
                    (a * a * p0.x + 2 * t * a * p1.x * w + t * t * p2.x) / u,
                    (a * a * p0.y + 2 * t * a * p1.y * w + t * t * p2.y) / u);
        }
    }
    return plist;
}
int jsfloor_i(double v) { return (int)floor(v); }
