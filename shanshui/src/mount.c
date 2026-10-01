// Mount.foot, mountain, flatMount, flatDec, distMount, rock.
#include "gen.h"

MountArgs mount_args(void) {
    MountArgs a;
    a.hei = a.wid = a.tex = a.veg = a.ret = a.cho = a.len = a.seg = a.sha = UNDEF;
    a.col = COL_NONE; a.has_col = 0;
    return a;
}

static const Col WHITE = {255, 255, 255, 2, 1};   // "white"

// ------------------------------------------------------------------- rock ---
static Col rock_col(double x, void *c) {
    (void)x; (void)c;
    return rgbaq(180, 180, 180, 0.3 + rnd() * 0.3, 3);
}
static double rock_dis(void *c) {
    (void)c;
    if (rnd() > 0.5) return 0.15 + 0.15 * rnd();
    return 0.85 - 0.15 * rnd();
}

void rock(double xoff, double yoff, double seed, const MountArgs *a) {
    double hei = ARG(a->hei, 80), wid = ARG(a->wid, 100), tex = ARG(a->tex, 40), sha = ARG(a->sha, 10);
    enum { R0 = 10, R1 = 50 };
    PLL ptlist = pll_new();
    for (int i = 0; i < R0; i++) {
        PL row = pl_new();
        double ns[R1];
        for (int j = 0; j < R1; j++) ns[j] = noise3(i, j * 0.2, seed);
        loopNoise(ns, R1);
        for (int j = 0; j < R1; j++) {
            double ang = ((double)j / R1) * M_PI * 2 - M_PI / 2;
            double c1 = hei * cos(ang), c2 = wid * sin(ang);
            double l = (wid * hei) / sqrt(c1 * c1 + c2 * c2);
            l *= 0.7 + 0.3 * ns[j];
            double p = 1 - (double)i / R0;
            double nx = cos(ang) * l * p;
            double ny = -sin(ang) * l * p;
            if (M_PI < ang || ang < 0) ny *= 0.2;
            ny += hei * ((double)i / R0) * 0.2;
            pl_push(&row, nx, ny);
        }
        pll_push(&ptlist, row);
    }
    // WHITE BG
    PL bg = pl_copy(ptlist.l[0]);
    pl_push(&bg, 0, 0);
    poly(bg, xoff, yoff, WHITE, COL_NONE, 0);
    // OUTLINE
    StrokeArgs s = stroke_args();
    s.col = rgba(100, 100, 100, 0.3); s.has_col = 1; s.noi = 1; s.wid = 3;
    stroke(pl_off(ptlist.l[0], xoff, yoff), &s);
    TexArgs t = tex_args();
    t.xof = xoff; t.yof = yoff; t.tex = tex; t.wid = 3; t.sha = sha;
    t.col = rock_col; t.dis = rock_dis;
    texture(ptlist, &t);
}

// ------------------------------------------------------------------- foot ---
static void foot(PLL ptlist, double xof, double yof) {
    PLL ftlist = pll_new();
    int span = 10;
    int ni = 0;
    for (int i = 0; i < ptlist.n - 2; i += 1) {
        if (i == ni) {
            static const double ch[2] = {1, 2};
            double rc = randChoiceD(ch, 2);
            ni = (int)fmin(ni + rc, ptlist.n - 1);
            PL f0 = pl_new(), f1 = pl_new();
            int L = ptlist.l[i].n;
            for (int j = 0; j < fmin(L / 8.0, 10); j++) {
                pl_push(&f0, ptlist.l[i].p[j].x + noise3(j * 0.1, i, 0) * 10, ptlist.l[i].p[j].y);
                pl_push(&f1, ptlist.l[i].p[L - 1 - j].x - noise3(j * 0.1, i, 0) * 10, ptlist.l[i].p[L - 1 - j].y);
            }
            pl_reverse(&f0);
            pl_reverse(&f1);
            for (int j = 0; j < span; j++) {
                double p = (double)j / span;
                double x1 = ptlist.l[i].p[0].x * (1 - p) + ptlist.l[ni].p[0].x * p;
                double y1 = ptlist.l[i].p[0].y * (1 - p) + ptlist.l[ni].p[0].y * p;
                double x2 = ptlist.l[i].p[L - 1].x * (1 - p) + ptlist.l[ni].p[L - 1].x * p;
                double y2 = ptlist.l[i].p[L - 1].y * (1 - p) + ptlist.l[ni].p[L - 1].y * p;
                double vib = -1.7 * (p - 1) * pow(p, 1.0 / 5);
                y1 += vib * 5 + noise3(xof * 0.05, i, 0) * 5;
                y2 += vib * 5 + noise3(xof * 0.05, i, 0) * 5;
                pl_push(&f0, x1, y1);
                pl_push(&f1, x2, y2);
            }
            pll_push(&ftlist, f0);
            pll_push(&ftlist, f1);
        }
    }
    for (int i = 0; i < ftlist.n; i++) poly(ftlist.l[i], xof, yof, WHITE, COL_NONE, 0);
    for (int j = 0; j < ftlist.n; j++) {
        double r = rnd();
        StrokeArgs s = stroke_args();
        s.col = rgbaq(100, 100, 100, 0.1 + r * 0.1, 3); s.has_col = 1; s.wid = 1;
        stroke(pl_off(ftlist.l[j], xof, yof), &s);
    }
}

// --------------------------------------------------------------- mountain ---
typedef struct { PLL pt; double xoff, yoff, seed, h; } VC;
typedef int (*GrowFn)(VC *v, int i, int j);
typedef int (*ProofFn)(const PL *veg, int i);
typedef void (*TreeFn)(VC *v, double x, double y);

static void vegetate(VC *v, GrowFn grow, ProofFn proof, TreeFn tree) {
    PL veglist = pl_new();
    for (int i = 0; i < v->pt.n; i += 1)
        for (int j = 0; j < v->pt.l[i].n; j += 1)
            if (grow(v, i, j)) pl_push(&veglist, v->pt.l[i].p[j].x, v->pt.l[i].p[j].y);
    for (int i = 0; i < veglist.n; i++)
        if (!proof || proof(&veglist, i)) tree(v, veglist.p[i].x, veglist.p[i].y);
}

static Col leafcol3(double x, double y, double base) {
    return rgbaq(100, 100, 100, noise3(0.01 * x, 0.01 * y, 0) * 0.5 * 0.3 + base, 3);
}

// RIM
static int g_rim(VC *v, int i, int j) {
    double ns = noise3(j * 0.1, v->seed, 0);
    return i == 0 && ns * ns * ns < 0.1 && fabs(v->pt.l[i].p[j].y) / v->h > 0.2;
}
static void t_rim(VC *v, double x, double y) {
    TreeArgs a = tree_args();
    a.col = leafcol3(x, y, 0.5); a.has_col = 1; a.clu = 2;
    EL(EL_TREES, tree02(x + v->xoff, y + v->yoff - 5, &a));
}
// TOP
static int g_top(VC *v, int i, int j) {
    double ns = noise3(i * 0.1, j * 0.1, v->seed + 2);
    return ns * ns * ns < 0.1 && fabs(v->pt.l[i].p[j].y) / v->h > 0.5;
}
static void t_top(VC *v, double x, double y) {
    TreeArgs a = tree_args();
    a.col = leafcol3(x, y, 0.5); a.has_col = 1;
    EL(EL_TREES, tree02(x + v->xoff, y + v->yoff, &a));
}
// MIDDLE
static int g_mid(VC *v, int i, int j) {
    double ns = noise3(i * 0.2, j * 0.05, v->seed);
    return (j % 2) && ns * ns * ns * ns < 0.012 && fabs(v->pt.l[i].p[j].y) / v->h < 0.3;
}
static int p_mid(const PL *vl, int i) {
    int counter = 0;
    for (int j = 0; j < vl->n; j++) {
        double dx = vl->p[i].x - vl->p[j].x, dy = vl->p[i].y - vl->p[j].y;
        if (i != j && dx * dx + dy * dy < 30 * 30) counter++;
        if (counter > 2) return 1;
    }
    return 0;
}
static void t_mid(VC *v, double x, double y) {
    double ht = ((v->h + y) / v->h) * 70;
    ht = ht * 0.3 + rnd() * ht * 0.7;
    double rw = rnd();
    TreeArgs a = tree_args();
    a.hei = ht; a.wid = rw * 3 + 1;
    a.col = leafcol3(x, y, 0.3); a.has_col = 1;
    EL(EL_TREES, tree01(x + v->xoff, y + v->yoff, &a));
}
// BOTTOM
static int g_bot(VC *v, int i, int j) {
    double ns = noise3(i * 0.2, j * 0.05, v->seed);
    return (j == 0 || j == v->pt.l[i].n - 1) && ns * ns * ns * ns < 0.012;
}
static double bot_ben(double x, void *c) { return x * *(double *)c; }   // pow(x*bc, 1)
static void t_bot(VC *v, double x, double y) {
    double ht = ((v->h + y) / v->h) * 120;
    ht = ht * 0.5 + rnd() * ht * 0.5;
    double bc = rnd() * 0.1;
    TreeArgs a = tree_args();
    a.hei = ht; a.ben = bot_ben; a.ctx = &bc;
    a.col = leafcol3(x, y, 0.3); a.has_col = 1;
    EL(EL_TREES, tree03(x + v->xoff, y + v->yoff, &a));
}
// BOTT ARCH
static int g_barch(VC *v, int i, int j) {
    double ns = noise3(i * 0.2, j * 0.05, v->seed + 10);
    return i != 0 && (j == 1 || j == v->pt.l[i].n - 2) && ns * ns * ns * ns < 0.008;
}
static void t_barch(VC *v, double x, double y) {
    static const int tts[6] = {0, 0, 1, 1, 1, 2};
    int tt = randChoiceI(tts, 6);
    if (tt == 1) {
        double w = normRand(40, 70);
        static const int sts[4] = {1, 2, 2, 3};
        int sto = randChoiceI(sts, 4);
        double rot = rnd();
        static const int sty3[3] = {1, 2, 3};
        int sty = randChoiceI(sty3, 3);
        ArchArgs a = arch_args();
        a.wid = w; a.sto = sto; a.rot = rot; a.sty = sty;
        EL(EL_BUILDINGS, arch02(x + v->xoff, y + v->yoff, v->seed, &a));
    } else if (tt == 2) {
        static const int sts[5] = {1, 1, 1, 2, 2};
        int sto = randChoiceI(sts, 5);
        ArchArgs a = arch_args();
        a.sto = sto;
        EL(EL_BUILDINGS, arch04(x + v->xoff, y + v->yoff, v->seed, &a));
    }
}
// TOP ARCH
static int g_tarch(VC *v, int i, int j) {
    return i == 1 && fabs(j - v->pt.l[i].n / 2.0) < 1 && rnd() < 0.02;
}
static void t_tarch(VC *v, double x, double y) {
    static const int sts[2] = {5, 7};
    int sto = randChoiceI(sts, 2);
    double r = rnd();
    ArchArgs a = arch_args();
    a.sto = sto; a.wid = 40 + r * 20;
    EL(EL_BUILDINGS, arch03(x + v->xoff, y + v->yoff, v->seed, &a));
}
// TRANSM
static int g_trans(VC *v, int i, int j) {
    double ns = noise3(i * 0.2, j * 0.05, v->seed + 20 * M_PI);
    return i % 2 == 0 && (j == 1 || j == v->pt.l[i].n - 2) && ns * ns * ns * ns < 0.002;
}
static void t_trans(VC *v, double x, double y) {
    ArchArgs a = arch_args();
    EL(EL_TOWERS, transmissionTower01(x + v->xoff, y + v->yoff, v->seed, &a));
}
// BOTT ROCK
static int g_rock(VC *v, int i, int j) {
    return (j == 0 || j == v->pt.l[i].n - 1) && rnd() < 0.1;
}
static void t_rock(VC *v, double x, double y) {
    double r1 = rnd(), r2 = rnd();
    MountArgs a = mount_args();
    a.wid = 20 + r1 * 20; a.hei = 20 + r2 * 20; a.sha = 2;
    EL(EL_ROCKS, rock(x + v->xoff, y + v->yoff, v->seed, &a));
}

void mountain(double xoff, double yoff, double seed, const MountArgs *a) {
    double hei = ISU(a->hei) ? 100 + rnd() * 400 : a->hei;
    double wid = ISU(a->wid) ? 400 + rnd() * 200 : a->wid;
    double tex = ARG(a->tex, 200);
    int veg = ISU(a->veg) ? 1 : (a->veg != 0);

    enum { R0 = 10, R1 = 50 };
    PLL ptlist = pll_new();
    double h = hei, w = wid;
    double hoff = 0;
    for (int j = 0; j < R0; j++) {
        hoff += (rnd() * yoff) / 100;
        PL row = pl_new();
        for (int i = 0; i < R1; i++) {
            double x = ((double)i / R1 - 0.5) * M_PI;
            double y = cos(x);
            y *= noise3(x + 10, j * 0.15, seed);
            double p = 1 - (double)j / R0;
            pl_push(&row, (x / M_PI) * w * p, -y * h * p + hoff);
        }
        pll_push(&ptlist, row);
    }
    VC v = {ptlist, xoff, yoff, seed, h};

    vegetate(&v, g_rim, NULL, t_rim);

    // WHITE BG
    PL bg = pl_copy(ptlist.l[0]);
    pl_push(&bg, 0, R0 * 4);
    poly(bg, xoff, yoff, WHITE, COL_NONE, 0);
    // OUTLINE
    StrokeArgs s = stroke_args();
    s.col = rgba(100, 100, 100, 0.3); s.has_col = 1; s.noi = 1; s.wid = 3;
    stroke(pl_off(ptlist.l[0], xoff, yoff), &s);

    foot(ptlist, xoff, yoff);
    {
        static const double shas[5] = {0, 0, 0, 0, 5};
        double sha = randChoiceD(shas, 5);
        TexArgs t = tex_args();
        t.xof = xoff; t.yof = yoff; t.tex = tex; t.sha = sha;
        texture(ptlist, &t);
    }

    vegetate(&v, g_top, NULL, t_top);
    if (veg) {
        vegetate(&v, g_mid, p_mid, t_mid);
        vegetate(&v, g_bot, NULL, t_bot);
    }
    vegetate(&v, g_barch, NULL, t_barch);
    vegetate(&v, g_tarch, NULL, t_tarch);
    vegetate(&v, g_trans, NULL, t_trans);
    vegetate(&v, g_rock, NULL, t_rock);
}

// --------------------------------------------------------------- flatDec ----
typedef struct { double xmin, xmax, ymin, ymax; } Bound;

static void rock_at(double xoff, double yoff, double sd, double w, double hh, double sha) {
    MountArgs a = mount_args();
    a.wid = w; a.hei = hh; a.sha = sha;
    EL(EL_ROCKS, rock(xoff, yoff, sd, &a));
}

static void flatDec(double xoff, double yoff, Bound g) {
    static const double tts[6] = {0, 0, 1, 2, 3, 4};
    int tt = (int)randChoiceD(tts, 6);

    for (int j = 0; j < rnd() * 5; j++) {
        double nx = normRand(g.xmin, g.xmax);
        double ny = normRand(-10, 10);
        double sd = rnd() * 100;
        double w = 10 + rnd() * 20, hh = 10 + rnd() * 20;
        rock_at(xoff + nx, yoff + (g.ymin + g.ymax) / 2 + ny + 10, sd, w, hh, 2);
    }
    static const double c0[4] = {0, 0, 1, 2};
    for (int j = 0; j < randChoiceD(c0, 4); j++) {
        double xr = xoff + normRand(g.xmin, g.xmax);
        double yr = yoff + (g.ymin + g.ymax) / 2 + normRand(-5, 5) + 20;
        for (int k = 0; k < 2 + rnd() * 3; k++) {
            double nr = normRand(-30, 30);
            double hh = 60 + rnd() * 40;
            TreeArgs a = tree_args();
            a.hei = hh;
            EL(EL_TREES, tree08(xr + fmin(fmax(nr, g.xmin), g.xmax), yr, &a));
        }
    }

    if (tt == 0) {
        for (int j = 0; j < rnd() * 3; j++) {
            double nx = normRand(g.xmin, g.xmax);
            double ny = normRand(-5, 5);
            double sd = rnd() * 100;
            double w = 50 + rnd() * 20, hh = 40 + rnd() * 20;
            rock_at(xoff + nx, yoff + (g.ymin + g.ymax) / 2 + ny + 20, sd, w, hh, 5);
        }
    }
    if (tt == 1) {
        double pmin = rnd() * 0.5;
        double pmax = rnd() * 0.5 + 0.5;
        double xmin = g.xmin * (1 - pmin) + g.xmax * pmin;
        double xmax = g.xmin * (1 - pmax) + g.xmax * pmax;
        for (double i = xmin; i < xmax; i += 30) {
            double nr = normRand(-1, 1);
            double hh = 100 + rnd() * 200;
            TreeArgs a = tree_args();
            a.hei = hh;
            EL(EL_TREES, tree05(xoff + i + 20 * nr, yoff + (g.ymin + g.ymax) / 2 + 20, &a));
        }
        for (int j = 0; j < rnd() * 4; j++) {
            double nx = normRand(g.xmin, g.xmax);
            double ny = normRand(-5, 5);
            double sd = rnd() * 100;
            double w = 50 + rnd() * 20, hh = 40 + rnd() * 20;
            rock_at(xoff + nx, yoff + (g.ymin + g.ymax) / 2 + ny + 20, sd, w, hh, 5);
        }
    } else if (tt == 2) {
        static const double c1[7] = {1, 1, 1, 1, 2, 2, 3};
        for (int i = 0; i < randChoiceD(c1, 7); i++) {
            double xr = normRand(g.xmin, g.xmax);
            double yr = (g.ymin + g.ymax) / 2;
            TreeArgs a = tree_args();
            EL(EL_TREES, tree04(xoff + xr, yoff + yr + 20, &a));
            for (int j = 0; j < rnd() * 2; j++) {
                double nr = normRand(-50, 50);
                double ny = normRand(-5, 5);
                double sd = j * i * rnd() * 100;
                double w = 50 + rnd() * 20, hh = 40 + rnd() * 20;
                rock_at(xoff + fmax(g.xmin, fmin(g.xmax, xr + nr)), yoff + yr + ny + 20, sd, w, hh, 5);
            }
        }
    } else if (tt == 3) {
        static const double c1[7] = {1, 1, 1, 1, 2, 2, 3};
        for (int i = 0; i < randChoiceD(c1, 7); i++) {
            double nx = normRand(g.xmin, g.xmax);
            double hh = 60 + rnd() * 60;
            TreeArgs a = tree_args();
            a.hei = hh;
            EL(EL_TREES, tree06(xoff + nx, yoff + (g.ymin + g.ymax) / 2, &a));
        }
    } else if (tt == 4) {
        double pmin = rnd() * 0.5;
        double pmax = rnd() * 0.5 + 0.5;
        double xmin = g.xmin * (1 - pmin) + g.xmax * pmin;
        double xmax = g.xmin * (1 - pmax) + g.xmax * pmax;
        for (double i = xmin; i < xmax; i += 20) {
            double nr = normRand(-1, 1);
            double ny = normRand(-1, 1);
            double hh = normRand(40, 80);
            TreeArgs a = tree_args();
            a.hei = hh;
            EL(EL_TREES, tree07(xoff + i + 20 * nr, yoff + (g.ymin + g.ymax) / 2 + ny + 0, &a));
        }
    }

    for (int i = 0; i < 50 * rnd(); i++) {
        double nx = normRand(g.xmin, g.xmax);
        double ny = normRand(g.ymin, g.ymax);
        TreeArgs a = tree_args();
        EL(EL_TREES, tree02(xoff + nx, yoff + ny, &a));
    }

    static const double c2[5] = {0, 0, 0, 0, 1};
    double ts = randChoiceD(c2, 5);
    if (ts == 1 && tt != 4) {
        double nx = normRand(g.xmin, g.xmax);
        double sd = rnd();
        double w = normRand(160, 200);
        double hh = normRand(80, 100);
        double per = rnd();
        ArchArgs a = arch_args();
        a.wid = w; a.hei = hh; a.per = per;
        EL(EL_BUILDINGS, arch01(xoff + nx, yoff + (g.ymin + g.ymax) / 2 + 20, sd, &a));
    }
}

// -------------------------------------------------------------- flatMount ---
static double flat_dis(void *c) {
    (void)c;
    if (rnd() > 0.5) return 0.1 + 0.4 * rnd();
    return 0.9 - 0.4 * rnd();
}

void flatMount(double xoff, double yoff, double seed, const MountArgs *a) {
    double hei = ISU(a->hei) ? 40 + rnd() * 400 : a->hei;
    double wid = ISU(a->wid) ? 400 + rnd() * 200 : a->wid;
    double tex = ARG(a->tex, 80), cho = ARG(a->cho, 0.5);

    enum { R0 = 5, R1 = 50 };
    PLL ptlist = pll_new();
    PLL flat = pll_new();
    double hoff = 0;
    for (int j = 0; j < R0; j++) {
        hoff += (rnd() * yoff) / 100;
        PL row = pl_new();
        PL fl = pl_new();
        for (int i = 0; i < R1; i++) {
            double x = ((double)i / R1 - 0.5) * M_PI;
            double y = cos(x * 2) + 1;
            y *= noise3(x + 10, j * 0.1, seed);
            double p = 1 - ((double)j / R0) * 0.6;
            double nx = (x / M_PI) * wid * p;
            double ny = -y * hei * p + hoff;
            double h = 100;
            if (ny < -h * cho + hoff) {
                ny = -h * cho + hoff;
                if (fl.n % 2 == 0) pl_push(&fl, nx, ny);
            } else {
                if (fl.n % 2 == 1) pl_push(&fl, row.p[row.n - 1].x, row.p[row.n - 1].y);
            }
            pl_push(&row, nx, ny);
        }
        pll_push(&ptlist, row);
        pll_push(&flat, fl);
    }

    PL bg = pl_copy(ptlist.l[0]);
    pl_push(&bg, 0, R0 * 4);
    poly(bg, xoff, yoff, WHITE, COL_NONE, 0);
    StrokeArgs s = stroke_args();
    s.col = rgba(100, 100, 100, 0.3); s.has_col = 1; s.noi = 1; s.wid = 3;
    stroke(pl_off(ptlist.l[0], xoff, yoff), &s);

    TexArgs t = tex_args();
    t.xof = xoff; t.yof = yoff; t.tex = tex; t.wid = 2; t.dis = flat_dis;
    texture(ptlist, &t);

    PL gr1 = pl_new(), gr2 = pl_new();
    for (int i = 0; i < flat.n; i += 2) {
        if (flat.l[i].n >= 2) {
            pl_push(&gr1, flat.l[i].p[0].x, flat.l[i].p[0].y);
            pl_push(&gr2, flat.l[i].p[flat.l[i].n - 1].x, flat.l[i].p[flat.l[i].n - 1].y);
        }
    }
    if (gr1.n == 0) return;
    double wb0 = gr1.p[0].x, wb1 = gr2.p[0].x;
    for (int i = 0; i < 3; i++) {
        double p = 0.8 - i * 0.2;
        pl_unshift(&gr1, wb0 * p, gr1.p[0].y - 5);
        pl_unshift(&gr2, wb1 * p, gr2.p[0].y - 5);
    }
    wb0 = gr1.p[gr1.n - 1].x;
    wb1 = gr2.p[gr2.n - 1].x;
    for (int i = 0; i < 3; i++) {
        double p = 0.6 - i * i * 0.1;
        pl_push(&gr1, wb0 * p, gr1.p[gr1.n - 1].y + 1);
        pl_push(&gr2, wb1 * p, gr2.p[gr2.n - 1].y + 1);
    }
    double d = 5;
    gr1 = div_pl(gr1, d);
    gr2 = div_pl(gr2, d);
    pl_reverse(&gr1);
    // grlist = gr1.reverse().concat(gr2.concat([gr1[0]]))
    PL gr = pl_cat(gr1, gr2);
    pl_push(&gr, gr1.p[0].x, gr1.p[0].y);
    int last = gr.n - 1;
    for (int i = 0; i < gr.n; i++) {
        double v = (1 - fabs(fmod(i, d) - d / 2) / (d / 2)) * 0.12;
        if (i == last) gr.p[last].x = gr.p[0].x;          // el original comparte el mismo objeto punto
        gr.p[i].x *= 1 - v + noise3(gr.p[i].y * 0.5, 0, 0) * v;
    }
    gr.p[0] = gr.p[last];
    poly(gr, xoff, yoff, WHITE, COL_NONE, 2);   // {str:"none", fil:"white", wid:2}
    StrokeArgs s2 = stroke_args();
    s2.wid = 3; s2.col = rgba(100, 100, 100, 0.2); s2.has_col = 1;
    stroke(pl_off(gr, xoff, yoff), &s2);

    Bound b = {1e300, -1e300, 1e300, -1e300};
    int first = 1;
    (void)first;
    for (int i = 0; i < gr.n; i++) {
        if (gr.p[i].x < b.xmin) b.xmin = gr.p[i].x;
        if (gr.p[i].x > b.xmax) b.xmax = gr.p[i].x;
        if (gr.p[i].y < b.ymin) b.ymin = gr.p[i].y;
        if (gr.p[i].y > b.ymax) b.ymax = gr.p[i].y;
    }
    flatDec(xoff, yoff, b);
}

// -------------------------------------------------------------- distMount ---
static Col dm_col(double x, double y, double yoff) {
    int c = (int)(noise3(x * 0.02, y * 0.02, yoff) * 55 + 200);
    return rgb3(c, c, c);
}

void distMount(double xoff, double yoff, double seed, const MountArgs *a) {
    double hei = ARG(a->hei, 300), len = ARG(a->len, 2000), seg = ARG(a->seg, 5);
    double span = 10;
    PLL ptlist = pll_new();
    for (int i = 0; i < len / span / seg; i++) {
        PL row = pl_new();
        for (int j = 0; j < seg + 1; j++) {
            double k = i * seg + j;
            pl_push(&row, xoff + k * span,
                    yoff - hei * noise3(k * 0.05, seed, 0) * sqrt(sin((M_PI * k) / (len / span))));
        }
        for (int j = 0; j < seg / 2 + 1; j++) {
            double k = i * seg + j * 2;
            pl_unshift(&row, xoff + k * span,
                       yoff + 24 * noise3(k * 0.05, 2, seed) * sin((M_PI * k) / (len / span)));
        }
        pll_push(&ptlist, row);
    }
    for (int i = 0; i < ptlist.n; i++) {
        PL row = ptlist.l[i];
        Pt lastp = row.p[row.n - 1];
        Col f = dm_col(lastp.x, lastp.y, yoff);
        poly(row, 0, 0, f, COL_NONE, 1);
        PLL T = triangulate(row, 100, 1, 0);
        for (int k = 0; k < T.n; k++) {
            Pt m = midPt(T.l[k].p, T.l[k].n);
            Col co = dm_col(m.x, m.y, yoff);
            poly(T.l[k], 0, 0, co, co, 1);
        }
    }
}
