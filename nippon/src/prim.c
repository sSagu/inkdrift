// Primitivas de dibujo: stroke (trazo de pincel), blob (hoja/mancha),
// texture (trazos de textura) y PolyTools.triangulate.
#include "gen.h"

double fn_sin_pi(double x, void *c) { (void)c; return sin(x * M_PI); }
double fn_one(double x, void *c) { (void)x; (void)c; return 1; }
static double fn_blob_default(double x, void *c) {
    (void)c;
    return x <= 1 ? sqrt(sin(x * M_PI)) : -sqrt(sin((x + 1) * M_PI));
}

// ---------------------------------------------------------------- stroke ----
StrokeArgs stroke_args(void) {
    StrokeArgs a;
    a.xof = a.yof = a.wid = a.noi = a.out = UNDEF;
    a.col = COL_NONE; a.has_col = 0;
    a.fun = NULL; a.ctx = NULL;
    return a;
}

void stroke(PL pt, const StrokeArgs *a) {
    double xof = ARG(a->xof, 0), yof = ARG(a->yof, 0);
    double wid = ARG(a->wid, 2);
    Col col = a->has_col ? a->col : rgba(200, 200, 200, 0.9);
    double noi = ARG(a->noi, 0.5);
    double out = ARG(a->out, 1);
    Fn fun = a->fun ? a->fun : fn_sin_pi;

    if (pt.n == 0) return;
    PL v0 = pl_new(), v1 = pl_new();
    double n0 = rnd() * 10;
    for (int i = 1; i < pt.n - 1; i++) {
        double w = wid * fun((double)i / pt.n, a->ctx);
        w = w * (1 - noi) + w * noi * noise3(i * 0.5, n0, 0);
        double a1 = atan2(pt.p[i].y - pt.p[i - 1].y, pt.p[i].x - pt.p[i - 1].x);
        double a2 = atan2(pt.p[i].y - pt.p[i + 1].y, pt.p[i].x - pt.p[i + 1].x);
        double ang = (a1 + a2) / 2;
        if (ang < a2) ang += M_PI;
        pl_push(&v0, pt.p[i].x + w * cos(ang), pt.p[i].y + w * sin(ang));
        pl_push(&v1, pt.p[i].x - w * cos(ang), pt.p[i].y - w * sin(ang));
    }
    // [p0] + v0 + reverse(v1 + [pLast]) + [p0]
    PL v = pl_new();
    pl_push(&v, pt.p[0].x, pt.p[0].y);
    for (int i = 0; i < v0.n; i++) pl_push(&v, v0.p[i].x, v0.p[i].y);
    pl_push(&v, pt.p[pt.n - 1].x, pt.p[pt.n - 1].y);
    for (int i = v1.n - 1; i >= 0; i--) pl_push(&v, v1.p[i].x, v1.p[i].y);
    pl_push(&v, pt.p[0].x, pt.p[0].y);
    poly(v, xof, yof, col, col, out);
}

void stroke_c(PL pts, Col col, double wid) {
    StrokeArgs a = stroke_args();
    a.col = col; a.has_col = 1; a.wid = wid;
    stroke(pts, &a);
}

// ------------------------------------------------------------------ blob ----
BlobArgs blob_args(void) {
    BlobArgs a;
    a.len = a.wid = a.ang = a.noi = a.ret = UNDEF;
    a.col = COL_NONE; a.has_col = 0;
    a.fun = NULL; a.ctx = NULL;
    return a;
}

PL blob(double x, double y, const BlobArgs *a) {
    double len = ARG(a->len, 20), wid = ARG(a->wid, 5), ang = ARG(a->ang, 0);
    Col col = a->has_col ? a->col : rgba(200, 200, 200, 0.9);
    double noi = ARG(a->noi, 0.5);
    double ret = ARG(a->ret, 0);
    Fn fun = a->fun ? a->fun : fn_blob_default;

    enum { RESO = 20 };
    double la_l[RESO + 1], la_a[RESO + 1], ns[RESO + 1];
    for (int i = 0; i < RESO + 1; i++) {
        double p = ((double)i / RESO) * 2;
        double xo = len / 2 - fabs(p - 1) * len;
        double yo = (fun(p, a->ctx) * wid) / 2;
        la_a[i] = atan2(yo, xo);
        la_l[i] = sqrt(xo * xo + yo * yo);
    }
    double n0 = rnd() * 10;
    for (int i = 0; i < RESO + 1; i++) ns[i] = noise3(i * 0.05, n0, 0);
    loopNoise(ns, RESO + 1);

    PL plist = pl_new();
    for (int i = 0; i < RESO + 1; i++) {
        double n = ns[i] * noi + (1 - noi);
        double nx = x + cos(la_a[i] + ang) * la_l[i] * n;
        double ny = y + sin(la_a[i] + ang) * la_l[i] * n;
        pl_push(&plist, nx, ny);
    }
    if (ret == 0) {
        poly(plist, 0, 0, col, col, 0);
        return pl_new();
    }
    return plist;
}

// --------------------------------------------------------------- texture ----
static double tex_noi_default(double x, void *c) { (void)c; return 30 / x; }
static Col tex_col_default(double x, void *c) {
    (void)x; (void)c;
    return rgbaq(100, 100, 100, rnd() * 0.3, 3);
}
static double tex_dis_default(void *c) {
    (void)c;
    if (rnd() > 0.5) return (1.0 / 3) * rnd();
    return (1.0 * 2) / 3 + (1.0 / 3) * rnd();
}

TexArgs tex_args(void) {
    TexArgs a;
    a.xof = a.yof = a.tex = a.wid = a.len = a.sha = UNDEF;
    a.noi = NULL; a.col = NULL; a.dis = NULL; a.ctx = NULL;
    return a;
}

void texture(PLL ptlist, const TexArgs *a) {
    double xof = ARG(a->xof, 0), yof = ARG(a->yof, 0);
    double tex = ARG(a->tex, 400), wid = ARG(a->wid, 1.5), len = ARG(a->len, 0.2);
    double sha = ARG(a->sha, 0);
    TexNoi noi = a->noi ? a->noi : tex_noi_default;
    TexCol col = a->col ? a->col : tex_col_default;
    TexDis dis = a->dis ? a->dis : tex_dis_default;

    int reso0 = ptlist.n, reso1 = ptlist.l[0].n;
    PLL texlist = pll_new();
    for (int i = 0; i < tex; i++) {
        int mid = (int)(dis(a->ctx) * reso1);
        int hlen = (int)floor(rnd() * (reso1 * len));
        int start = mid - hlen, end = mid + hlen;
        start = start < 0 ? 0 : (start > reso1 ? reso1 : start);
        end = end < 0 ? 0 : (end > reso1 ? reso1 : end);

        double layer = ((double)i / tex) * (reso0 - 1);
        PL row = pl_new();
        for (int j = start; j < end; j++) {
            double p = layer - floor(layer);
            Pt lo = ptlist.l[(int)floor(layer)].p[j], hi = ptlist.l[(int)ceil(layer)].p[j];
            double x = lo.x * p + hi.x * (1 - p);
            double y = lo.y * p + hi.y * (1 - p);
            double n0 = noi(layer + 1, a->ctx) * (noise3(x, j * 0.5, 0) - 0.5);
            double n1 = noi(layer + 1, a->ctx) * (noise3(y, j * 0.5, 0) - 0.5);
            pl_push(&row, x + n0, y + n1);
        }
        pll_push(&texlist, row);
    }
    // SHADE
    if (sha != 0) {
        for (int j = 0; j < texlist.n; j += 2) {
            StrokeArgs s = stroke_args();
            s.col = rgba(100, 100, 100, 0.1); s.has_col = 1; s.wid = sha;
            stroke(pl_off(texlist.l[j], xof, yof), &s);
        }
    }
    // TEXTURE
    for (int j = 0 + (int)sha; j < texlist.n; j += 1 + (int)sha) {
        PL shifted = pl_off(texlist.l[j], xof, yof);
        StrokeArgs s = stroke_args();
        s.col = col((double)j / texlist.n, a->ctx); s.has_col = 1; s.wid = wid;
        stroke(shifted, &s);
    }
}

// ------------------------------------------------------------- triangulate --
static void sides_of(PL p, double *s) {
    for (int i = 0; i < p.n; i++) {
        Pt pt = p.p[i], np = p.p[i != p.n - 1 ? i + 1 : 0];
        double dx = np.x - pt.x, dy = np.y - pt.y;
        s[i] = sqrt(dx * dx + dy * dy);
    }
}
static double area_of(PL p) {
    double s[256];
    sides_of(p, s);
    double a = s[0], b = s[1], c = s[2];
    double h = (a + b + c) / 2;
    return sqrt(h * (h - a) * (h - b) * (h - c));
}
static double sliver_ratio(PL p) {
    double s[256];
    sides_of(p, s);
    double A = area_of(p);
    double P = 0;
    for (int i = 0; i < p.n; i++) P = P + s[i];
    return A / P;
}

static void shatter(PL p, double a, PLL *out, int depth) {
    if (p.n == 0) return;
    if (area_of(p) < a || depth > 40) { pll_push(out, p); return; }
    double s[256];
    sides_of(p, s);
    int ind = 0;
    for (int i = 0; i < p.n; i++) if (s[i] > s[ind]) ind = i;
    int nind = (ind + 1) % p.n, lind = (ind + 2) % p.n;
    Pt two[2] = {p.p[ind], p.p[nind]};
    Pt mid = midPt(two, 2);
    PL t1 = pl_new(), t2 = pl_new();
    pl_push(&t1, p.p[ind].x, p.p[ind].y); pl_push(&t1, mid.x, mid.y); pl_push(&t1, p.p[lind].x, p.p[lind].y);
    pl_push(&t2, p.p[lind].x, p.p[lind].y); pl_push(&t2, p.p[nind].x, p.p[nind].y); pl_push(&t2, mid.x, mid.y);
    shatter(t1, a, out, depth + 1);
    shatter(t2, a, out, depth + 1);
}

// Solo la rama convex=true del original (es la unica que se usa).
PLL triangulate(PL plist, double area, int convex, int optimize) {
    (void)convex;
    PLL out = pll_new();
    PL cur = pl_copy(plist);
    while (cur.n > 3) {
        PL best_tri = cur, best_rest = pl_new();
        int have_best = 0;
        double best_ratio = 0;
        for (int i = 0; i < cur.n; i++) {
            Pt pt = cur.p[i];
            Pt lp = cur.p[i != 0 ? i - 1 : cur.n - 1];
            Pt np = cur.p[i != cur.n - 1 ? i + 1 : 0];
            PL tri = pl_new();
            pl_push(&tri, lp.x, lp.y); pl_push(&tri, pt.x, pt.y); pl_push(&tri, np.x, np.y);
            PL rest = pl_copy(cur);
            pl_splice(&rest, i, 1);
            if (!optimize) { best_tri = tri; best_rest = rest; have_best = 1; break; }
            double r = sliver_ratio(tri);
            if (r >= best_ratio) { best_tri = tri; best_rest = rest; have_best = 1; best_ratio = r; }
        }
        (void)have_best;
        // sin ninguna oreja valida: best = [plist, []]
        shatter(best_tri, area, &out, 0);
        cur = best_rest;
    }
    shatter(cur, area, &out, 0);
    return out;
}
