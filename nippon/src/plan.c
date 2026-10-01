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
enum { T_MOUNT, T_DISTMOUNT, T_FLATMOUNT, T_BOAT, T_VILLAGE, T_DUEL, T_DRSTONE, T_BRIDGE };

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
    // Japon: aldeas, duelos y, muy de vez en cuando, el guino a Dr. Stone (en primer plano)
    // (aldeas, duelos y la escena de Dr. Stone van solo sobre llanuras: ver flatDec)

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

static int spot_clear(const Reg *regs, int nreg, double x, double y) {
    for (int k = 0; k < nreg; k++)
        if ((regs[k].tag == T_MOUNT || regs[k].tag == T_FLATMOUNT) && fabs(regs[k].x - x) < 330 && fabs(regs[k].y - y) < 150) return 0;
    for (int k = 0; k < g_world.n; k++) {
        const Chunk *c = g_world.chunks[k];
        if ((!strcmp(c->tag, "mount") || !strcmp(c->tag, "flatmount")) && c->y > 0 &&
            fabs(c->x - x) < 330 && fabs(c->y - y) < 150) return 0;
    }
    return 1;
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
                if (rnd() < 0.45) { double kx = (rnd() - 0.5) * 300, ky = 25 + rnd() * 30; EL(EL_KOI, koi_pond(p->x + kx, p->y + ky)); }   // Japon
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
                double rr = rnd();
                if (rr < 0.04) { double mh = 70 + rnd() * 30; EL(EL_EASTER, mecha(p->x + len / 2, p->y + 14, mh)); }   // easter egg
                else if (rr < 0.34) { double fh = 150 + rnd() * 70; EL(EL_FUJI, fuji(p->x + len / 2, p->y + 10, sd, fh, len * 0.9 + 300)); }  // Japon
                else EL(EL_DISTANT, distMount(p->x, p->y, sd, &a));
                world_add(c);
            } else if (p->tag == T_BOAT) {
                Chunk *c = new_chunk("boat", p->x, p->y);
                double sd = rnd();
                double rf = rnd();
                ArchArgs a = arch_args();
                a.sca = p->y / 800;
                a.fli = floor(2 * rf) == 0 ? 1 : 0;
                (void)sd;
                EL(EL_BOATS, jboat(p->x, p->y, a.sca, (int)a.fli));   // Japon: barca de proa levantada
                world_add(c);
            } else if (p->tag == T_VILLAGE) {
                if (!spot_clear(regs, nreg, p->x, p->y)) { ar_reset(m2); continue; }
                Chunk *c = new_chunk("village", p->x, p->y);
                EL(EL_VILLAGES, { islet(p->x - 20, p->y + 12, 420); village(p->x, p->y); });
                world_add(c);
            } else if (p->tag == T_BRIDGE) {
                if (!spot_clear(regs, nreg, p->x, p->y)) { ar_reset(m2); continue; }
                Chunk *c = new_chunk("bridge", p->x, p->y);
                double bl = 50 + rnd() * 50;
                EL(EL_BRIDGES, bridged_islets(p->x, p->y, bl));
                world_add(c);
            } else if (p->tag == T_DUEL) {
                if (!spot_clear(regs, nreg, p->x, p->y)) { ar_reset(m2); continue; }
                Chunk *c = new_chunk("duel", p->x, p->y);
                EL(EL_SAMURAI, { islet(p->x + 20, p->y + 7, 190); duel(p->x, p->y, 0.45); });
                if (rnd() < 0.5) EL(EL_BAMBOO, bamboo(p->x + 60, p->y + 3, 70, 4));
                world_add(c);
            } else if (p->tag == T_DRSTONE) {
                if (!spot_clear(regs, nreg, p->x, p->y)) { ar_reset(m2); continue; }
                Chunk *c = new_chunk("drstone", p->x, p->y);
                EL(EL_EASTER, { islet(p->x - 5, p->y + 8, 170); stone_couple(p->x, p->y); });
                world_add(c);
            }
            ar_reset(m2);
        }
        {   // Japon: cielo (nubes y bruma) detras de todo; tambien arriba del horizonte para cuando te alejas
            size_t m3 = ar_mark();
            double sx = w->xmax - w->cwid;   // el trozo que se acaba de planificar
            Chunk *c = new_chunk("sky", sx, -20000);
            int nk = (int)(rnd() * 3.4);
            for (int k = 0; k < nk; k++) { double kx = sx + rnd() * w->cwid, ky = -500 + rnd() * 720, ks = 1.6 + rnd() * 1.8; EL(EL_CLOUDS, kumo(kx, ky, ks)); }
            if (rnd() < 0.6) { double kx = sx + rnd() * w->cwid, ky = -300 + rnd() * 560, kl = 260 + rnd() * 320; EL(EL_CLOUDS, kasumi(kx, ky, kl)); }
            world_add(c);
            ar_reset(m3);
            // olas: solo en mar abierto (sin montanas cerca), y pocas
            if (rnd() < 0.5) {
                double ox = sx + rnd() * w->cwid, oy = 520 + rnd() * 170;
                int clear = 1;
                for (int k = 0; k < nreg && clear; k++)
                    if ((regs[k].tag == T_MOUNT || regs[k].tag == T_FLATMOUNT) && fabs(regs[k].x - ox) < 260 && fabs(regs[k].y - oy) < 140) clear = 0;
                for (int k = 0; k < g_world.n && clear; k++) {
                    const Chunk *q = g_world.chunks[k];
                    if ((!strcmp(q->tag, "mount") || !strcmp(q->tag, "flatmount")) && q->y > 0 && fabs(q->x - ox) < 260 && fabs(q->y - oy) < 140) clear = 0;
                }
                if (clear) {
                    Chunk *cw = new_chunk("wave", ox, oy);
                    double ws = 0.7 + rnd() * 0.5;
                    EL(EL_WAVES, nami(ox, oy, ws));
                    world_add(cw);
                    ar_reset(m3);
                }
            }
        }
        // Japon: puentes que unen dos montanas (islas) vecinas, apoyados en sus pies
        char *used = calloc((size_t)(nreg ? nreg : 1), 1);
        for (int i = 0; i < nreg; i++) {
            if (regs[i].tag != T_MOUNT || used[i]) continue;
            for (int j = 0; j < nreg; j++) {
                if (j == i || regs[j].tag != T_MOUNT || used[j]) continue;
                double dx = regs[j].x - regs[i].x, dy = regs[j].y - regs[i].y;
                if (dx < 540 || dx > 760 || fabs(dy) > 35) continue;
                double bx = (regs[i].x + regs[j].x) / 2, by = fmax(regs[i].y, regs[j].y) + 6;
                int blocked = 0;                              // el tramo tiene que ser agua libre
                for (int k = 0; k < nreg && !blocked; k++)
                    if (k != i && k != j && (regs[k].tag == T_MOUNT || regs[k].tag == T_FLATMOUNT) &&
                        fabs(regs[k].x - bx) < 300 && regs[k].y > by - 70 && regs[k].y < by + 20) blocked = 1;
                for (int k = 0; k < g_world.n && !blocked; k++) {
                    const Chunk *c = g_world.chunks[k];
                    if ((!strcmp(c->tag, "mount") || !strcmp(c->tag, "flatmount") || !strcmp(c->tag, "bridge")) && c->y > 0 &&
                        fabs(c->x - bx) < 300 && c->y > by - 70 && c->y < by + 20 &&
                        !(fabs(c->x - regs[i].x) < 1 && fabs(c->y - regs[i].y) < 1) &&
                        !(fabs(c->x - regs[j].x) < 1 && fabs(c->y - regs[j].y) < 1)) blocked = 1;
                }
                if (blocked || rnd() > 0.6) continue;
                used[i] = used[j] = 1;
                size_t m3 = ar_mark();
                double y = fmax(regs[i].y, regs[j].y) + 6;
                double x = (regs[i].x + regs[j].x) / 2;
                double len = dx - 430;                        // de pie a pie, montado sobre ambos
                Chunk *c = new_chunk("bridge", x, y);
                EL(EL_BRIDGES, bridge(x, y, len));
                world_add(c);
                ar_reset(m3);
                break;
            }
        }
        free(used);
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