// mountplanner, MEM, chunkloader.
#include "plan.h"
#include "gen.h"
#include "paper.h"

// ------------------------------------------------------------ planmtx -------
// Arreglo JS indexado por enteros (posiblemente negativos) donde "undefined" + 1
// da NaN y `x || 0` convierte NaN/undefined en 0. Se modela con NaN = undefined.
typedef struct { long long key; double val; int used; } PmSlot;
static PmSlot *g_pm;
static size_t g_pm_cap, g_pm_n;

static size_t pm_hash(long long k) { return (size_t)((unsigned long long)k * 11400714819323198485ull >> 20); }

static PmSlot *pm_find(long long key, int create) {
    if (!g_pm_cap) {
        if (!create) return NULL;
        g_pm_cap = 1 << 14;
        g_pm = calloc(g_pm_cap, sizeof(PmSlot));
    }
    if (create && (g_pm_n + 1) * 2 > g_pm_cap) {      // crecer
        PmSlot *old = g_pm;
        size_t oc = g_pm_cap;
        g_pm_cap *= 2;
        g_pm = calloc(g_pm_cap, sizeof(PmSlot));
        g_pm_n = 0;
        for (size_t i = 0; i < oc; i++)
            if (old[i].used) { PmSlot *s = pm_find(old[i].key, 1); s->val = old[i].val; }
        free(old);
    }
    size_t i = pm_hash(key) & (g_pm_cap - 1);
    for (;;) {
        if (!g_pm[i].used) {
            if (!create) return NULL;
            g_pm[i].used = 1; g_pm[i].key = key; g_pm[i].val = NAN; g_pm_n++;
            return &g_pm[i];
        }
        if (g_pm[i].key == key) return &g_pm[i];
        i = (i + 1) & (g_pm_cap - 1);
    }
}
static double pm_get(long long k) { PmSlot *s = pm_find(k, 0); return s ? s->val : NAN; }
static void pm_set(long long k, double v) { pm_find(k, 1)->val = v; }

// Descarta entradas muy a la izquierda (nunca se vuelven a consultar).
static void pm_prune(long long below) {
    if (!g_pm_cap) return;
    PmSlot *old = g_pm;
    size_t oc = g_pm_cap;
    g_pm_cap = 1 << 14;
    while (g_pm_cap < g_pm_n) g_pm_cap *= 2;
    g_pm = calloc(g_pm_cap, sizeof(PmSlot));
    g_pm_n = 0;
    for (size_t i = 0; i < oc; i++)
        if (old[i].used && old[i].key >= below) { PmSlot *s = pm_find(old[i].key, 1); s->val = old[i].val; }
    free(old);
}

// ------------------------------------------------------------- planner ------
typedef struct { int tag; double x, y; } Reg;
enum { T_MOUNT, T_DISTMOUNT, T_FLATMOUNT, T_BOAT };

typedef struct { Reg *r; int n, cap; } RegList;

static int chadd(RegList *reg, Reg r, double mind) {
    for (int k = 0; k < reg->n; k++)
        if (fabs(reg->r[k].x - r.x) < mind) return 0;
    if (reg->n == reg->cap) {
        reg->cap = reg->cap ? reg->cap * 2 : 16;
        Reg *nr = ar_alloc((size_t)reg->cap * sizeof(Reg));
        if (reg->n) memcpy(nr, reg->r, (size_t)reg->n * sizeof(Reg));
        reg->r = nr;
    }
    reg->r[reg->n++] = r;
    return 1;
}

static const double SAMP = 0.03;
static double ns_f(double x, double y) { (void)y; return fmax(noise3(x * SAMP, 0, 0) - 0.55, 0) * 2; }
static double yr_f(double x) { return noise3(x * 0.01, M_PI, 0); }

static int locmax(double x, double y, double r) {
    double z0 = ns_f(x, y);
    if (z0 <= 0.3) return 0;
    for (double i = x - r; i < x + r; i++)
        for (double j = y - r; j < y + r; j++)
            if (ns_f(i, j) > z0) return 0;
    return 1;
}

static RegList mountplanner(double xmin, double xmax) {
    RegList reg = {0, 0, 0};
    double xstep = 5, mwid = 200;
    for (double i = xmin; i < xmax; i += xstep) {
        long long i1 = (long long)floor(i / xstep);
        double v = pm_get(i1);
        pm_set(i1, (isnan(v) || v == 0) ? 0 : v);
    }
    for (double i = xmin; i < xmax; i += xstep) {
        for (double j = 0; j < yr_f(i) * 480; j += 30) {
            if (locmax(i, j, 2)) {
                double rr = rnd();
                double xof = i + 2 * (rr - 0.5) * 500;
                double yof = j + 300;
                Reg r = {T_MOUNT, xof, yof};
                if (chadd(&reg, r, 10)) {
                    for (double k = floor((xof - mwid) / xstep); k < (xof + mwid) / xstep; k++)
                        pm_set((long long)k, pm_get((long long)k) + 1);
                }
            }
        }
        if (fmod(fabs(i), 1000) < fmax(1, xstep - 1)) {
            double ry = 280 - rnd() * 50;
            Reg r = {T_DISTMOUNT, i, ry};
            chadd(&reg, r, 10);
        }
    }
    for (double i = xmin; i < xmax; i += xstep) {
        if (pm_get((long long)floor(i / xstep)) == 0) {
            if (rnd() < 0.01) {
                for (int j = 0; j < 4 * rnd(); j++) {
                    double rx = i + 2 * (rnd() - 0.5) * 700;
                    Reg r = {T_FLATMOUNT, rx, 700 - j * 50};
                    chadd(&reg, r, 10);
                }
            }
        }
    }
    for (double i = xmin; i < xmax; i += xstep) {
        if (rnd() < 0.2) {
            double ry = 300 + rnd() * 390;
            Reg r = {T_BOAT, i, ry};
            chadd(&reg, r, 400);
        }
    }
    return reg;
}

// --------------------------------------------------------------- world ------
World g_world = {NULL, 0, 0, 0, 0, 512};

static void world_add(Chunk *nch) {
    World *w = &g_world;
    if (w->n == w->cap) {
        w->cap = w->cap ? w->cap * 2 : 64;
        w->chunks = realloc(w->chunks, (size_t)w->cap * sizeof(Chunk *));
    }
    int pos = -1;
    if (w->n == 0) pos = 0;
    else if (nch->y <= w->chunks[0]->y) pos = 0;
    else if (nch->y >= w->chunks[w->n - 1]->y) pos = w->n;
    else {
        for (int j = 0; j < w->n - 1; j++)
            if (w->chunks[j]->y <= nch->y && nch->y <= w->chunks[j + 1]->y) { pos = j + 1; break; }
    }
    if (pos < 0) { chunk_free(nch); free(nch); return; }
    memmove(&w->chunks[pos + 1], &w->chunks[pos], (size_t)(w->n - pos) * sizeof(Chunk *));
    w->chunks[pos] = nch;
    w->n++;
}

static Chunk *new_chunk(const char *tag, double x, double y) {
    Chunk *c = malloc(sizeof(Chunk));
    chunk_init(c, tag, x, y);
    g_cur = c;
    return c;
}

void world_load(double xmin, double xmax) {
    World *w = &g_world;
    while (xmax > w->xmax - w->cwid || xmin < w->xmin + w->cwid) {
        RegList plan;
        size_t mark = ar_mark();
        if (xmax > w->xmax - w->cwid) {
            plan = mountplanner(w->xmax, w->xmax + w->cwid);
            w->xmax = w->xmax + w->cwid;
        } else {
            plan = mountplanner(w->xmin - w->cwid, w->xmin);
            w->xmin = w->xmin - w->cwid;
        }
        // la lista del plan vive en la arena: se copia antes de generar (cada generador la reinicia)
        Reg *regs = malloc((size_t)(plan.n ? plan.n : 1) * sizeof(Reg));
        memcpy(regs, plan.r, (size_t)plan.n * sizeof(Reg));
        int nreg = plan.n;
        ar_reset(mark);

        for (int i = 0; i < nreg; i++) {
            Reg *p = &regs[i];
            size_t m2 = ar_mark();
            if (p->tag == T_MOUNT) {
                Chunk *c = new_chunk("mount", p->x, p->y);
                double sd = i * 2 * rnd();
                MountArgs a = mount_args();
                mountain(p->x, p->y, sd, &a);
                world_add(c);
                ar_reset(m2);
                Chunk *c2 = new_chunk("mount", p->x, p->y - 10000);
                EL(EL_WATER, water(p->x, p->y, i * 2, UNDEF, UNDEF, UNDEF));
                world_add(c2);
            } else if (p->tag == T_FLATMOUNT) {
                Chunk *c = new_chunk("flatmount", p->x, p->y);
                double sd = 2 * rnd() * M_PI;
                double rw = rnd(), rc = rnd();
                MountArgs a = mount_args();
                a.wid = 600 + rw * 400; a.hei = 100; a.cho = 0.5 + rc * 0.2;
                EL(EL_FLAT, flatMount(p->x, p->y, sd, &a));
                world_add(c);
            } else if (p->tag == T_DISTMOUNT) {
                Chunk *c = new_chunk("distmount", p->x, p->y);
                double sd = rnd() * 100;
                static const double lens[3] = {500, 1000, 1500};
                double len = randChoiceD(lens, 3);
                MountArgs a = mount_args();
                a.hei = 150; a.len = len;
                EL(EL_DISTANT, distMount(p->x, p->y, sd, &a));
                world_add(c);
            } else if (p->tag == T_BOAT) {
                Chunk *c = new_chunk("boat", p->x, p->y);
                double sd = rnd();
                double rf = rnd();
                ArchArgs a = arch_args();
                a.sca = p->y / 800;
                a.fli = floor(2 * rf) == 0 ? 1 : 0;
                EL(EL_BOATS, boat01(p->x, p->y, sd, &a));
                world_add(c);
            }
            ar_reset(m2);
        }
        free(regs);
    }
}

void world_reset(void) {
    World *w = &g_world;
    for (int i = 0; i < w->n; i++) { chunk_free(w->chunks[i]); free(w->chunks[i]); }
    w->n = 0;
    w->xmin = w->xmax = 0;
    free(g_pm);
    g_pm = NULL; g_pm_cap = g_pm_n = 0;
    noise_reset();
}

void world_prune(double keep_x) {
    World *w = &g_world;
    int k = 0;
    for (int i = 0; i < w->n; i++) {
        if (w->chunks[i]->x < keep_x) { chunk_free(w->chunks[i]); free(w->chunks[i]); }
        else w->chunks[k++] = w->chunks[i];
    }
    w->n = k;
    pm_prune((long long)floor(keep_x / 5) - 400);
}

// banco de pruebas: el equivalente a chunkloader(0, 3000) del original
void gen_initial_chunks(FILE *out) {
    world_load(0, 3000);
    for (int i = 0; i < g_world.n; i++) chunk_dump(out, g_world.chunks[i]);
    fprintf(out, "RNG|%.0f\n", g_prng_s);
}

void gen_scroll_test(FILE *out, int steps) {
    world_load(0, 3000);
    static uint8_t paper[512 * 512 * 3];
    paper_make(paper);
    double cursx = 0;
    for (int s = 1; s <= steps; s++) {
        cursx += 200;
        world_load(cursx, cursx + 3000);
    }
    for (int i = 0; i < g_world.n; i++) chunk_dump(out, g_world.chunks[i]);
    fprintf(out, "RNG|%.0f\n", g_prng_s);
}