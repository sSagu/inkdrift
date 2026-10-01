// Tree.tree01 .. tree08 (y branch, twig, barkify).
// Cada rnd() se saca a una variable local en el MISMO orden que en el JS original,
// porque el orden de evaluacion de argumentos en C no esta garantizado.
#include "gen.h"

TreeArgs tree_args(void) {
    TreeArgs a;
    a.hei = a.wid = a.noi = a.clu = UNDEF;
    a.col = COL_NONE; a.has_col = 0;
    a.ben = NULL; a.ctx = NULL;
    return a;
}

static Col tree_col(const TreeArgs *a) { return a->has_col ? a->col : rgba(100, 100, 100, 0.5); }

// col.includes("rgba(") ? col sin "rgba(" separado por comas : ["100","100","100", defa]
static void leafcol(Col col, int *r, int *g, int *b, double *al, double defa) {
    if (col_is_rgba(col)) { *r = col.r; *g = col.g; *b = col.b; *al = col.a; }
    else { *r = *g = *b = 100; *al = defa; }
}

static double leaf_fun(double x, void *c) {      // hoja de twig / tree02
    (void)c;
    return x <= 1 ? sqrt(sin(x * M_PI) * x) : -sqrt(sin((x - 2) * M_PI * (x - 2)));
}

// ------------------------------------------------------------------ tree01 --
void tree01(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 50), wid = ARG(a->wid, 3);
    Col col = tree_col(a);
    double reso = 10;
    double ns[10][2];
    for (int i = 0; i < reso; i++) { ns[i][0] = noise3(i * 0.5, 0, 0); ns[i][1] = noise3(i * 0.5, 0.5, 0); }
    int lr, lg, lb; double la;
    leafcol(col, &lr, &lg, &lb, &la, 0.5);
    PL line1 = pl_new(), line2 = pl_new();
    for (int i = 0; i < reso; i++) {
        double nx = x;
        double ny = y - (i * hei) / reso;
        if (i >= reso / 4) {
            for (int j = 0; j < (reso - i) / 5; j++) {
                double r1 = rnd(), r2 = rnd(), r3 = rnd(), r4 = rnd(), r5 = rnd(), r6 = rnd();
                BlobArgs b = blob_args();
                b.len = r3 * 20 * (reso - i) * 0.2 + 10;
                b.wid = r4 * 6 + 3;
                b.ang = ((r5 - 0.5) * M_PI) / 6;
                b.col = rgbaq(lr, lg, lb, r6 * 0.2 + la, 1); b.has_col = 1;
                blob(nx + (r1 - 0.5) * wid * 1.2 * (reso - i), ny + (r2 - 0.5) * wid, &b);
            }
        }
        pl_push(&line1, nx + (ns[i][0] - 0.5) * wid - wid / 2, ny);
        pl_push(&line2, nx + (ns[i][1] - 0.5) * wid + wid / 2, ny);
    }
    poly(line1, 0, 0, COL_NONE, col, 1.5);
    poly(line2, 0, 0, COL_NONE, col, 1.5);
}

// ------------------------------------------------------------------ tree02 --
void tree02(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 16), wid = ARG(a->wid, 8), clu = ARG(a->clu, 5);
    Col col = tree_col(a);
    for (int i = 0; i < clu; i++) {
        double g1 = randGaussian();
        double bx = x + g1 * clu * 4;
        double g2 = randGaussian();
        double by = y + g2 * clu * 4;
        double r1 = rnd(), r2 = rnd();
        BlobArgs b = blob_args();
        b.ang = M_PI / 2;
        b.fun = leaf_fun;
        b.wid = r1 * wid * 0.75 + wid * 0.5;
        b.len = r2 * hei * 0.75 + hei * 0.5;
        b.col = col; b.has_col = 1;
        blob(bx, by, &b);
    }
}

// ------------------------------------------------------------------ tree03 --
static double tree03_shape(double x) { return log(50 * x + 1) / 3.95; }

void tree03(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 50), wid = ARG(a->wid, 5);
    Col col = tree_col(a);
    double reso = 10;
    double ns[10][2];
    for (int i = 0; i < reso; i++) { ns[i][0] = noise3(i * 0.5, 0, 0); ns[i][1] = noise3(i * 0.5, 0.5, 0); }
    int lr, lg, lb; double la;
    leafcol(col, &lr, &lg, &lb, &la, 0.5);

    Chunk blobs;
    chunk_init(&blobs, "", 0, 0);
    Chunk *prev = g_cur;
    PL line1 = pl_new(), line2 = pl_new();
    for (int i = 0; i < reso; i++) {
        double bn = a->ben ? a->ben(i / reso, a->ctx) : 0;
        double nx = x + bn * 100;
        double ny = y - (i * hei) / reso;
        if (i >= reso / 5) {
            for (int j = 0; j < (reso - i) * 2; j++) {
                double r1 = rnd();
                double ox = r1 * wid * 2 * tree03_shape((reso - i) / reso);
                static const double pm[2] = {-1, 1};
                double ch = randChoiceD(pm, 2);
                double r3 = rnd(), r4 = rnd(), r5 = rnd(), r6 = rnd();
                BlobArgs b = blob_args();
                b.len = ox * 2;
                b.wid = r4 * 6 + 3;
                b.ang = ((r5 - 0.5) * M_PI) / 6;
                b.col = rgbaq(lr, lg, lb, r6 * 0.2 + la, 3); b.has_col = 1;
                g_cur = &blobs;
                blob(nx + ox * ch, ny + (r3 - 0.5) * wid * 2, &b);
                g_cur = prev;
            }
        }
        pl_push(&line1, nx + (((ns[i][0] - 0.5) * wid - wid / 2) * (reso - i)) / reso, ny);
        pl_push(&line2, nx + (((ns[i][1] - 0.5) * wid + wid / 2) * (reso - i)) / reso, ny);
    }
    pl_reverse(&line2);
    PL lc = pl_cat(line1, line2);
    poly(lc, 0, 0, COL_WHITE, col, 1.5);
    chunk_append(g_cur, &blobs);
}

// ------------------------------------------------------------------ branch --
typedef struct { PL a, b; } Branch;

static Branch branch(double hei, double wid, double ang, double det, double ben) {
    PL tlist = pl_new();
    double nx = 0, ny = 0;
    pl_push(&tlist, nx, ny);
    double a0 = 0;
    double g = 3;
    for (int i = 0; i < g; i++) {
        double r = rnd();
        static const double pm[2] = {-1, 1};
        double ch = randChoiceD(pm, 2);
        a0 += (ben / 2 + (r * ben) / 2) * ch;
        nx += (cos(a0) * hei) / g;
        ny -= (sin(a0) * hei) / g;
        pl_push(&tlist, nx, ny);
    }
    double ta = atan2(tlist.p[tlist.n - 1].y, tlist.p[tlist.n - 1].x);
    for (int i = 0; i < tlist.n; i++) {
        double aa = atan2(tlist.p[i].y, tlist.p[i].x);
        double d = sqrt(tlist.p[i].x * tlist.p[i].x + tlist.p[i].y * tlist.p[i].y);
        tlist.p[i].x = d * cos(aa - ta + ang);
        tlist.p[i].y = d * sin(aa - ta + ang);
    }
    Branch br = {pl_new(), pl_new()};
    double span = det;
    double tl = (tlist.n - 1) * span;
    double lx = 0, ly = 0;
    for (int i = 0; i < tl; i += 1) {
        Pt lastp = tlist.p[(int)floor(i / span)], nextp = tlist.p[(int)ceil(i / span)];
        double p = fmod(i, span) / span;
        double bx = lastp.x * (1 - p) + nextp.x * p;
        double by = lastp.y * (1 - p) + nextp.y * p;
        double an = atan2(by - ly, bx - lx);
        double woff = ((noise3(i * 0.3, 0, 0) - 0.5) * wid * hei) / 80;
        double b = 0;
        if (p == 0) b = rnd() * wid;
        double nw = wid * (((tl - i) / tl) * 0.5 + 0.5);
        pl_push(&br.a, bx + cos(an + M_PI / 2) * (nw + woff + b), by + sin(an + M_PI / 2) * (nw + woff + b));
        pl_push(&br.b, bx + cos(an - M_PI / 2) * (nw - woff + b), by + sin(an - M_PI / 2) * (nw - woff + b));
        lx = bx;
        ly = by;
    }
    return br;
}

// -------------------------------------------------------------------- twig --
static double fn_cos_half_pi(double x, void *c) { (void)c; return cos((x * M_PI) / 2); }

static void twig(double tx, double ty, int dep, double dir, double sca, double wid, double ang,
                 int lea0, double lea1) {
    PL twlist = pl_new();
    int tl = 10;
    double hs = rnd() * 0.5 + 0.5;
    {   // tfun = randChoice([fun2]) consume un rnd()
        static const double one[1] = {0};
        (void)randChoiceD(one, 1);
    }
    double a0 = ((rnd() * M_PI) / 6) * dir + ang;
    for (int i = 0; i < tl; i++) {
        double tf = -1 / pow((double)i / tl + 1, 5) + 1;      // fun2(i / tl)
        double mx = dir * tf * 50 * sca * hs;
        double my = -i * 5 * sca;
        double aa = atan2(my, mx);
        double d = sqrt(mx * mx + my * my);
        double nx = cos(aa + a0) * d;
        double ny = sin(aa + a0) * d;
        pl_push(&twlist, nx + tx, ny + ty);
        if ((i == (int)(tl / 3) || i == (int)((tl * 2) / 3)) && dep > 0) {
            static const double pm[2] = {-1, 1};
            double ch = randChoiceD(pm, 2);
            twig(nx + tx, ny + ty, dep - 1, dir * ch, sca * 0.8, wid, ang, lea0, lea1);
        }
        if (i == tl - 1 && lea0) {
            for (int j = 0; j < 5; j++) {
                double dj = (j - 2.5) * 5;
                double r1 = rnd(), r2 = rnd(), r3 = rnd();
                BlobArgs b = blob_args();
                b.wid = (6 + 3 * r1) * wid;
                b.len = (15 + 12 * r2) * wid;
                b.ang = ang / 2 + M_PI / 2 + M_PI * 0.2 * (r3 - 0.5);
                b.col = rgbaq(100, 100, 100, 0.5 + dep * 0.2, 3); b.has_col = 1;
                b.fun = leaf_fun;
                blob(nx + tx + cos(ang) * dj * wid, ny + ty + (sin(ang) * dj - lea1 / (dep + 1)) * wid, &b);
            }
        }
    }
    StrokeArgs s = stroke_args();
    s.wid = 1;
    s.fun = fn_cos_half_pi;
    s.col = rgba(100, 100, 100, 0.5); s.has_col = 1;
    stroke(twlist, &s);
}

// ----------------------------------------------------------------- barkify --
static double fn_bark(double x, void *c) {
    double fr = *(double *)c;
    return sin((x + fr) * M_PI * 3);
}

static void bark(double x, double y, double wid, double ang) {
    double r_len = rnd();
    double len = 10 + 10 * r_len;
    double noi = 0.5;
    enum { RESO = 20 };
    double la_l[RESO + 1], la_a[RESO + 1], ns[RESO + 1];
    for (int i = 0; i < RESO + 1; i++) {
        double p = ((double)i / RESO) * 2;
        double xo = len / 2 - fabs(p - 1) * len;
        double fp = p <= 1 ? sqrt(sin(p * M_PI)) : -sqrt(sin((p + 1) * M_PI));
        double yo = (fp * wid) / 2;
        la_a[i] = atan2(yo, xo);
        la_l[i] = sqrt(xo * xo + yo * yo);
    }
    double n0 = rnd() * 10;
    for (int i = 0; i < RESO + 1; i++) ns[i] = noise3(i * 0.05, n0, 0);
    loopNoise(ns, RESO + 1);
    PL brk = pl_new();
    for (int i = 0; i < RESO + 1; i++) {
        double n = ns[i] * noi + (1 - noi);
        pl_push(&brk, x + cos(la_a[i] + ang) * la_l[i] * n, y + sin(la_a[i] + ang) * la_l[i] * n);
    }
    double fr = rnd();
    StrokeArgs s = stroke_args();
    s.wid = 0.8; s.noi = 0; s.out = 0;
    s.col = rgba(100, 100, 100, 0.4); s.has_col = 1;
    s.fun = fn_bark; s.ctx = &fr;
    stroke(brk, &s);
}

// Muta los puntos de tr0/tr1 (el original comparte los objetos punto entre listas).
static void barkify(double x, double y, PL *tr0, PL *tr1) {
    for (int i = 2; i < tr0->n - 1; i++) {
        double a0 = atan2(tr0->p[i].y - tr0->p[i - 1].y, tr0->p[i].x - tr0->p[i - 1].x);
        double a1 = atan2(tr1->p[i].y - tr1->p[i - 1].y, tr1->p[i].x - tr1->p[i - 1].x);
        double p = rnd();
        double nx = tr0->p[i].x * (1 - p) + tr1->p[i].x * p;
        double ny = tr0->p[i].y * (1 - p) + tr1->p[i].y * p;
        double r2 = rnd();
        if (r2 < 0.2) {
            BlobArgs b = blob_args();
            b.noi = 1; b.len = 15; b.wid = 6 - fabs(p - 0.5) * 10; b.ang = (a0 + a1) / 2;
            b.col = rgba(100, 100, 100, 0.6); b.has_col = 1;
            blob(nx + x, ny + y, &b);
        } else {
            bark(nx + x, ny + y, 5 - fabs(p - 0.5) * 10, (a0 + a1) / 2);
        }
        double r3 = rnd();
        if (r3 < 0.05) {
            double jl = rnd() * 2 + 2;
            // xya = randChoice([[tr0[i], a0], [tr1[i], a1]])
            double r4 = rnd();
            int pick = (int)floor(2 * r4);
            double px = pick == 0 ? tr0->p[i].x : tr1->p[i].x;
            double py = pick == 0 ? tr0->p[i].y : tr1->p[i].y;
            double pa = pick == 0 ? a0 : a1;
            for (int j = 0; j < jl; j++) {
                double r5 = rnd();
                BlobArgs b = blob_args();
                b.wid = 4; b.len = 4 + 6 * r5; b.ang = a0 + M_PI / 2;
                b.col = rgba(100, 100, 100, 0.6); b.has_col = 1;
                blob(px + x + cos(pa) * (j - jl / 2) * 4, py + y + sin(pa) * (j - jl / 2) * 4, &b);
            }
        }
    }
    // trflist = tr0 + reverse(tr1)
    int n0 = tr0->n, n1 = tr1->n, nt = n0 + n1;
    Pt **orig = ar_alloc((size_t)(nt ? nt : 1) * sizeof(Pt *));
    for (int t = 0; t < nt; t++) orig[t] = t < n0 ? &tr0->p[t] : &tr1->p[n1 - 1 - (t - n0)];
    // rglist = [[]]; para cada punto: con prob 0.5 se abre una fila nueva, si no se agrega
    int *rowstart = ar_alloc((size_t)(nt + 2) * sizeof(int));
    int *rowlen = ar_alloc((size_t)(nt + 2) * sizeof(int));
    Pt **rowpts = ar_alloc((size_t)(nt ? nt : 1) * sizeof(Pt *));
    int nrows = 1, cnt = 0;
    rowstart[0] = 0; rowlen[0] = 0;
    for (int t = 0; t < nt; t++) {
        double r = rnd();
        if (r < 0.5) {
            rowstart[nrows] = cnt; rowlen[nrows] = 0; nrows++;
        } else {
            rowpts[cnt++] = orig[t];
            rowlen[nrows - 1]++;
        }
    }
    for (int i = 0; i < nrows; i++) {
        PL row = pl_new();
        for (int k = 0; k < rowlen[i]; k++) pl_push(&row, rowpts[rowstart[i] + k]->x, rowpts[rowstart[i] + k]->y);
        PL d = div_pl(row, 4);
        for (int j = 0; j < d.n; j++) {
            double g1 = randGaussian();
            d.p[j].x += (noise3(i, j * 0.1, 1) - 0.5) * (15 + 5 * g1);
            double g2 = randGaussian();
            d.p[j].y += (noise3(i, j * 0.1, 2) - 0.5) * (15 + 5 * g2);
        }
        if (rowlen[i] > 0) *rowpts[rowstart[i] + rowlen[i] - 1] = d.p[d.n - 1];   // el ultimo punto es el objeto original
        StrokeArgs s = stroke_args();
        s.wid = 1.5; s.out = 0;
        s.col = rgba(100, 100, 100, 0.7); s.has_col = 1;
        stroke(pl_off(d, x, y), &s);
    }
}

// ----------------------------------------------------------- tronco comun ---
static double fn_sin1(double x, void *c) { (void)x; (void)c; return sin(1); }

// poly + stroke finales de tree04/05/06 (el trazo tiene alfa 0.4 + rnd()*0.1)
static void trunk_finish(PL trmlist, double x, double y, Col col, double alpha_base) {
    poly(trmlist, x, y, COL_WHITE, col, 0);
    pl_splice(&trmlist, 0, 1);
    pl_splice(&trmlist, trmlist.n - 1, 1);
    double r = rnd();
    StrokeArgs s = stroke_args();
    s.col = rgbaq(100, 100, 100, alpha_base + r * 0.1, 3); s.has_col = 1;
    s.wid = 2.5; s.fun = fn_sin1; s.noi = 0.9; s.out = 0;
    stroke(pl_off(trmlist, x, y), &s);
}

// ------------------------------------------------------------------ tree04 --
void tree04(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 300), wid = ARG(a->wid, 6);
    Col col = tree_col(a);
    Chunk tx, tw;
    chunk_init(&tx, "", 0, 0);
    chunk_init(&tw, "", 0, 0);
    Chunk *prev = g_cur;

    Branch tr = branch(hei, wid, -M_PI / 2, 10, M_PI * 0.2);
    g_cur = &tx; barkify(x, y, &tr.a, &tr.b); g_cur = prev;
    pl_reverse(&tr.b);
    PL trlist = pl_cat(tr.a, tr.b);

    PL trmlist = pl_new();
    for (int i = 0; i < trlist.n; i++) {
        int cond = (i >= trlist.n * 0.3 && i <= trlist.n * 0.7 && rnd() < 0.1) || i == trlist.n / 2.0 - 1;
        if (cond) {
            double ba = M_PI * 0.2 - M_PI * 1.4 * (i > trlist.n / 2.0);
            double rr = rnd();
            Branch br = branch(hei * (rr + 1) * 0.3, wid * 0.5, ba, 10, M_PI * 0.2);
            pl_splice(&br.a, 0, 1);
            pl_splice(&br.b, 0, 1);
            PL o0 = pl_off(br.a, trlist.p[i].x, trlist.p[i].y), o1 = pl_off(br.b, trlist.p[i].x, trlist.p[i].y);
            g_cur = &tx; barkify(x, y, &o0, &o1); g_cur = prev;
            for (int j = 0; j < br.a.n; j++) {
                double r = rnd();
                if (r < 0.2 || j == br.a.n - 1) {
                    g_cur = &tw;
                    twig(br.a.p[j].x + trlist.p[i].x + x, br.a.p[j].y + trlist.p[i].y + y, 1,
                         ba > -M_PI / 2 ? 1 : -1, (0.5 * hei) / 300, hei / 300,
                         ba > -M_PI / 2 ? ba : ba + M_PI, 1, 12);
                    g_cur = prev;
                }
            }
            pl_reverse(&br.b);
            PL bl = pl_cat(br.a, br.b);
            PL sh = pl_off(bl, trlist.p[i].x, trlist.p[i].y);
            trmlist = pl_cat(trmlist, sh);
        } else {
            pl_push(&trmlist, trlist.p[i].x, trlist.p[i].y);
        }
    }
    trunk_finish(trmlist, x, y, col, 0.4);
    chunk_append(g_cur, &tx);
    chunk_append(g_cur, &tw);
}

// ------------------------------------------------------------------ tree05 --
void tree05(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 300), wid = ARG(a->wid, 5);
    Col col = tree_col(a);
    Chunk tx, tw;
    chunk_init(&tx, "", 0, 0);
    chunk_init(&tw, "", 0, 0);
    Chunk *prev = g_cur;

    Branch tr = branch(hei, wid, -M_PI / 2, 10, 0);
    g_cur = &tx; barkify(x, y, &tr.a, &tr.b); g_cur = prev;
    pl_reverse(&tr.b);
    PL trlist = pl_cat(tr.a, tr.b);

    PL trmlist = pl_new();
    for (int i = 0; i < trlist.n; i++) {
        double p = fabs(i - trlist.n * 0.5) / (trlist.n * 0.5);
        int cond;
        if (i >= trlist.n * 0.2 && i <= trlist.n * 0.8 && i % 3 == 0) {
            double r = rnd();
            cond = r > p;
        } else cond = 0;
        cond = cond || i == trlist.n / 2.0 - 1;
        if (cond) {
            double bar = rnd() * 0.2;
            double ba = -bar * M_PI - (1 - bar * 2) * M_PI * (i > trlist.n / 2.0);
            double rr = rnd();
            Branch br = branch(hei * (0.3 * p - rr * 0.05), wid * 0.5, ba, 10, 0.5);
            pl_splice(&br.a, 0, 1);
            pl_splice(&br.b, 0, 1);
            for (int j = 0; j < br.a.n; j++) {
                if (j % 20 == 0 || j == br.a.n - 1) {
                    g_cur = &tw;
                    twig(br.a.p[j].x + trlist.p[i].x + x, br.a.p[j].y + trlist.p[i].y + y, 0,
                         ba > -M_PI / 2 ? 1 : -1, (0.2 * hei) / 300, hei / 300,
                         ba > -M_PI / 2 ? ba : ba + M_PI, 1, 5);
                    g_cur = prev;
                }
            }
            pl_reverse(&br.b);
            PL bl = pl_cat(br.a, br.b);
            PL sh = pl_off(bl, trlist.p[i].x, trlist.p[i].y);
            trmlist = pl_cat(trmlist, sh);
        } else {
            pl_push(&trmlist, trlist.p[i].x, trlist.p[i].y);
        }
    }
    trunk_finish(trmlist, x, y, col, 0.4);
    chunk_append(g_cur, &tx);
    chunk_append(g_cur, &tw);
}

// ------------------------------------------------------------------ tree06 --
typedef struct { Chunk tx, tw; Chunk *outer; } T6;

static PL frac_tree6(T6 *t, double xoff, double yoff, int dep, double hei, double wid, double ang, double ben) {
    Branch tr = branch(hei, wid, ang, hei / 20, ben);
    t->outer = g_cur;
    g_cur = &t->tx; barkify(xoff, yoff, &tr.a, &tr.b); g_cur = t->outer;
    pl_reverse(&tr.b);
    PL trlist = pl_cat(tr.a, tr.b);

    PL trmlist = pl_new();
    for (int i = 0; i < trlist.n; i++) {
        double r0 = rnd();
        int c1 = (r0 < 0.025 && i >= trlist.n * 0.2 && i <= trlist.n * 0.8);
        int half = (int)(trlist.n / 2.0);
        int cond = (c1 || i == half - 1 || i == half + 1) && dep > 0;
        if (cond) {
            double bar = 0.02 + rnd() * 0.08;
            double ba = bar * M_PI - bar * 2 * M_PI * (i > trlist.n / 2.0);
            double rr = rnd();
            PL brlist = frac_tree6(t, trlist.p[i].x + xoff, trlist.p[i].y + yoff, dep - 1,
                                   hei * (0.7 + rr * 0.2), wid * 0.6, ang + ba, 0.55);
            for (int j = 0; j < brlist.n; j++) {
                double r = rnd();
                if (r < 0.03) {
                    double r2 = rnd();
                    g_cur = &t->tw;
                    twig(brlist.p[j].x + trlist.p[i].x + xoff, brlist.p[j].y + trlist.p[i].y + yoff, 2,
                         ba > 0 ? 1 : -1, 0.3, 1, ba * (r2 * 0.5 + 0.75), 0, 0);
                    g_cur = t->outer;
                }
            }
            PL sh = pl_off(brlist, trlist.p[i].x, trlist.p[i].y);
            trmlist = pl_cat(trmlist, sh);
        } else {
            pl_push(&trmlist, trlist.p[i].x, trlist.p[i].y);
        }
    }
    return trmlist;
}

void tree06(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 100), wid = ARG(a->wid, 6);
    Col col = tree_col(a);
    T6 t;
    chunk_init(&t.tx, "", 0, 0);
    chunk_init(&t.tw, "", 0, 0);
    PL trmlist = frac_tree6(&t, x, y, 3, hei, wid, -M_PI / 2, 0);
    trunk_finish(trmlist, x, y, col, 0.4);
    chunk_append(g_cur, &t.tx);
    chunk_append(g_cur, &t.tw);
}

// ------------------------------------------------------------------ tree07 --
static double tree07_ben_default(double x, void *c) { (void)c; return sqrt(x) * 0.2; }
static double tree07_fun(double x, void *c) {
    (void)c;
    return x <= 1 ? 2.75 * x * pow(1 - x, 1 / 1.8) : 2.75 * (x - 2) * pow(x - 1, 1 / 1.8);
}

void tree07(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 60), wid = ARG(a->wid, 4);
    Fn ben = a->ben ? a->ben : tree07_ben_default;
    Col col = a->has_col ? a->col : rgba(100, 100, 100, 1);
    double reso = 10;
    double ns[10][2];
    for (int i = 0; i < reso; i++) { ns[i][0] = noise3(i * 0.5, 0, 0); ns[i][1] = noise3(i * 0.5, 0.5, 0); }
    int lr, lg, lb; double la;
    leafcol(col, &lr, &lg, &lb, &la, 1);
    PLL T = pll_new();
    PL line1 = pl_new(), line2 = pl_new();
    for (int i = 0; i < reso; i++) {
        double nx = x + ben(i / reso, a->ctx) * 100;
        double ny = y - (i * hei) / reso;
        if (i >= reso / 4) {
            for (int j = 0; j < 1; j++) {
                double r1 = rnd(), r2 = rnd(), r3 = rnd(), r4 = rnd(), r5 = rnd();
                BlobArgs b = blob_args();
                b.len = r3 * 50 + 20;
                b.wid = r4 * 12 + 12;
                b.ang = (-r5 * M_PI) / 6;
                b.col = rgbaq(lr, lg, lb, la, 3); b.has_col = 1;
                b.fun = tree07_fun;
                b.ret = 1;
                PL bpl = blob(nx + (r1 - 0.5) * wid * 1.2 * (reso - i) * 0.5, ny + (r2 - 0.5) * wid * 0.5, &b);
                PLL tt = triangulate(bpl, 50, 1, 0);
                for (int k = 0; k < tt.n; k++) pll_push(&T, tt.l[k]);
            }
        }
        pl_push(&line1, nx + (ns[i][0] - 0.5) * wid - wid / 2, ny);
        pl_push(&line2, nx + (ns[i][1] - 0.5) * wid + wid / 2, ny);
    }
    pl_reverse(&line2);
    PLL trunk = triangulate(pl_cat(line1, line2), 50, 1, 1);
    for (int k = 0; k < T.n; k++) pll_push(&trunk, T.l[k]);
    T = trunk;
    for (int k = 0; k < T.n; k++) {
        Pt m = midPt(T.l[k].p, T.l[k].n);
        int c = (int)(noise3(m.x * 0.02, m.y * 0.02, 0) * 200 + 50);
        Col co = rgba(c, c, c, 0.8);
        poly(T.l[k], 0, 0, co, co, 0);
    }
}

// ------------------------------------------------------------------ tree08 --
static double fn_cos_half(double x, void *c) { (void)c; return cos(0.5 * M_PI * x); }
static double fn_ret1(double x, void *c) { (void)x; (void)c; return 1; }

static void frac_tree8(double xoff, double yoff, int dep, double ang, double len, double ben) {
    Fn fun = dep == 0 ? fn_cos_half : fn_ret1;
    Pt spt = {xoff, yoff};
    Pt ept = {xoff + cos(ang) * len, yoff + sin(ang) * len};
    PL trm = pl_new();
    pl_push(&trm, xoff, yoff);
    pl_push(&trm, xoff + len, yoff);
    double rb = rnd();
    int bsign = floor(2 * rb) == 0 ? 1 : -1;       // [sin(x*PI), -sin(x*PI)]
    trm = div_pl(trm, 10);
    for (int i = 0; i < trm.n; i++) trm.p[i].y += bsign * sin(((double)i / trm.n) * M_PI) * 2;
    for (int i = 0; i < trm.n; i++) {
        double d = distance(trm.p[i], spt);
        double aa = atan2(trm.p[i].y - spt.y, trm.p[i].x - spt.x);
        trm.p[i].x = spt.x + d * cos(aa + ang);
        trm.p[i].y = spt.y + d * sin(aa + ang);
    }
    StrokeArgs s = stroke_args();
    s.fun = fun; s.wid = 0.8;
    s.col = rgba(100, 100, 100, 0.5); s.has_col = 1;
    stroke(trm, &s);
    if (dep != 0) {
        static const double pm[2] = {-1, 1};
        double rc = randChoiceD(pm, 2);
        double nben = ben + rc * M_PI * 0.001 * dep * dep;
        double r = rnd();
        if (r < 0.5) {
            double n1 = normRand(-1, 0.5), n2 = normRand(0.5, 1);
            double pick = floor(2 * rnd()) == 0 ? n1 : n2;
            double ang1 = ang + ben + M_PI * pick * 0.2;
            double len1 = len * normRand(0.8, 0.9);
            frac_tree8(ept.x, ept.y, dep - 1, ang1, len1, nben);
            double m1 = normRand(-1, -0.5), m2 = normRand(0.5, 1);
            double pick2 = floor(2 * rnd()) == 0 ? m1 : m2;
            double ang2 = ang + ben + M_PI * pick2 * 0.2;
            double len2 = len * normRand(0.8, 0.9);
            frac_tree8(ept.x, ept.y, dep - 1, ang2, len2, nben);
        } else {
            double len1 = len * normRand(0.8, 0.9);
            frac_tree8(ept.x, ept.y, dep - 1, ang + ben, len1, nben);
        }
    }
}

void tree08(double x, double y, const TreeArgs *a) {
    double hei = ARG(a->hei, 80), wid = ARG(a->wid, 1);
    Col col = tree_col(a);
    Chunk tw;
    chunk_init(&tw, "", 0, 0);
    Chunk *prev = g_cur;

    double ang = normRand(-1, 1) * M_PI * 0.2;
    Branch tr = branch(hei, wid, -M_PI / 2 + ang, hei / 20, M_PI * 0.2);
    pl_reverse(&tr.b);
    PL trlist = pl_cat(tr.a, tr.b);

    for (int i = 0; i < trlist.n; i++) {
        double r = rnd();
        if (r < 0.2) {
            int dep = (int)floor(4 * rnd());
            double r2 = rnd();
            g_cur = &tw;
            frac_tree8(x + trlist.p[i].x, y + trlist.p[i].y, dep, -M_PI / 2 - ang * r2, 15, 0);
            g_cur = prev;
        } else if (i == (int)floor(trlist.n / 2.0)) {
            g_cur = &tw;
            frac_tree8(x + trlist.p[i].x, y + trlist.p[i].y, 3, -M_PI / 2 + ang, 15, 0);
            g_cur = prev;
        }
    }
    poly(trlist, x, y, COL_WHITE, col, 0);
    double r = rnd();
    StrokeArgs s = stroke_args();
    s.col = rgbaq(100, 100, 100, 0.6 + r * 0.1, 3); s.has_col = 1;
    s.wid = 2.5; s.fun = fn_sin1; s.noi = 0.9; s.out = 0;
    stroke(pl_off(trlist, x, y), &s);
    chunk_append(g_cur, &tw);
}
