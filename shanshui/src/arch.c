// Arch (hut, box, deco, rail, roof, pagroof, arch01..04, boat01, transmissionTower01),
// Man y water.
#include "gen.h"

ArchArgs arch_args(void) {
    ArchArgs a;
    a.hei = a.wid = a.rot = a.per = a.sto = a.sty = a.rai = a.len = a.sca = a.fli = UNDEF;
    return a;
}

static const Col WHITE = {255, 255, 255, 2, 1};

// atajo para listas literales: pts({x0,y0,x1,y1,...})
#define LIT(...) pl_lit((int)(sizeof((double[]){__VA_ARGS__}) / sizeof(double) / 2), (double[]){__VA_ARGS__})

static PL flip0(PL p) {           // flip(ptlist) con axis = 0, en el lugar
    for (int i = 0; i < p.n; i++) p.p[i].x = 0 - (p.p[i].x - 0);
    return p;
}

// ------------------------------------------------------------------- hut ----
static Col hut_col(double x, void *c) { (void)x; (void)c; return rgbaq(120, 120, 120, 0.3 + rnd() * 0.3, 3); }
static double hut_a2(double a) { return a * a; }
static double hut_dis(void *c) { (void)c; return wtrand(hut_a2); }
static double hut_noi(double x, void *c) { (void)x; (void)c; return 5; }

static void hut(double xoff, double yoff, double hei, double wid) {
    double tex = 300;
    enum { R = 10 };
    PLL ptlist = pll_new();
    for (int i = 0; i < R; i++) {
        PL row = pl_new();
        double heir = hei + hei * 0.2 * rnd();
        for (int j = 0; j < R; j++) {
            double nx = wid * ((double)i / (R - 1) - 0.5) * pow((double)j / (R - 1), 0.7);
            double ny = heir * ((double)j / (R - 1));
            pl_push(&row, nx, ny);
        }
        pll_push(&ptlist, row);
    }
    PL last = pl_slice(ptlist.l[R - 1], 0, -1);
    pl_reverse(&last);
    poly(pl_cat(pl_slice(ptlist.l[0], 0, -1), last), xoff, yoff, WHITE, COL_NONE, 0);
    poly(ptlist.l[0], xoff, yoff, COL_NONE, rgba(100, 100, 100, 0.3), 2);
    poly(ptlist.l[R - 1], xoff, yoff, COL_NONE, rgba(100, 100, 100, 0.3), 2);
    TexArgs t = tex_args();
    t.xof = xoff; t.yof = yoff; t.tex = tex; t.wid = 1; t.len = 0.25;
    t.col = hut_col; t.dis = hut_dis; t.noi = hut_noi;
    texture(ptlist, &t);
}

// ------------------------------------------------------------------ deco ----
typedef struct { int style, hsp0, hsp1, vsp0, vsp1; } DecSpec;

static PL div2(Pt a, Pt b, double reso) {
    PL l = pl_new();
    pl_push(&l, a.x, a.y);
    pl_push(&l, b.x, b.y);
    return div_pl(l, reso);
}

static PLL deco(DecSpec ds, Pt pul, Pt pur, Pt pdl, Pt pdr) {
    PLL plist = pll_new();
    PL dl = div2(pul, pdl, ds.vsp1), dr = div2(pur, pdr, ds.vsp1);
    PL du = div2(pul, pur, ds.hsp1), dd = div2(pdl, pdr, ds.hsp1);
    int style = ds.style;
    if (style == 1) {
        Pt mlu = du.p[ds.hsp0], mru = du.p[du.n - 1 - ds.hsp0];
        Pt mld = dd.p[ds.hsp0], mrd = dd.p[du.n - 1 - ds.hsp0];
        for (int i = ds.vsp0; i < dl.n - ds.vsp0; i += ds.vsp0) {
            Pt mml = div2(mlu, mld, ds.vsp1).p[i];
            Pt mmr = div2(mru, mrd, ds.vsp1).p[i];
            Pt ml = dl.p[i], mr = dr.p[i];
            pll_push(&plist, div2(mml, ml, 5));
            pll_push(&plist, div2(mmr, mr, 5));
        }
        pll_push(&plist, div2(mlu, mld, 5));
        pll_push(&plist, div2(mru, mrd, 5));
    } else if (style == 2) {
        for (int i = ds.hsp0; i < du.n - ds.hsp0; i += ds.hsp0) pll_push(&plist, div2(du.p[i], dd.p[i], 5));
    } else if (style == 3) {
        Pt mlu = du.p[ds.hsp0], mru = du.p[du.n - 1 - ds.hsp0];
        Pt mld = dd.p[ds.hsp0], mrd = dd.p[du.n - 1 - ds.hsp0];
        for (int i = ds.vsp0; i < dl.n - ds.vsp0; i += ds.vsp0) {
            Pt mml = div2(mlu, mld, ds.vsp1).p[i];
            Pt mmr = div2(mru, mrd, ds.vsp1).p[i];
            Pt mmu = div2(mlu, mru, ds.vsp1).p[i];
            Pt mmd = div2(mld, mrd, ds.vsp1).p[i];
            pll_push(&plist, div2(mml, mmr, 5));
            pll_push(&plist, div2(mmu, mmd, 5));
        }
        pll_push(&plist, div2(mlu, mld, 5));
        pll_push(&plist, div2(mru, mrd, 5));
    }
    return plist;
}

// ------------------------------------------------------------------- box ----
typedef struct {
    double hei, wid, rot, per, wei;
    int tra, bot;
    DecSpec dec;       // style 0 = sin decoracion
} BoxArgs;

static void box(double xoff, double yoff, BoxArgs b) {
    double hei = b.hei, wid = b.wid, rot = b.rot, per = b.per, wei = b.wei;
    double mid = -wid * 0.5 + wid * rot;
    double bmid = -wid * 0.5 + wid * (1 - rot);
    PLL ptlist = pll_new();
    Pt A = {-wid * 0.5, -hei}, B = {-wid * 0.5, 0}, C = {wid * 0.5, -hei}, D = {wid * 0.5, 0};
    pll_push(&ptlist, div2(A, B, 5));
    pll_push(&ptlist, div2(C, D, 5));
    if (b.bot) {
        pll_push(&ptlist, div2(B, (Pt){mid, per}, 5));
        pll_push(&ptlist, div2(D, (Pt){mid, per}, 5));
    }
    pll_push(&ptlist, div2((Pt){mid, -hei}, (Pt){mid, per}, 5));
    if (b.tra) {
        if (b.bot) {
            pll_push(&ptlist, div2(B, (Pt){bmid, -per}, 5));
            pll_push(&ptlist, div2(D, (Pt){bmid, -per}, 5));
        }
        pll_push(&ptlist, div2((Pt){bmid, -hei}, (Pt){bmid, -per}, 5));
    }
    double surf = (rot < 0.5) * 2 - 1;
    if (b.dec.style) {
        PLL dd = deco(b.dec, (Pt){surf * wid * 0.5, -hei}, (Pt){mid, -hei + per},
                      (Pt){surf * wid * 0.5, 0}, (Pt){mid, per});
        for (int i = 0; i < dd.n; i++) pll_push(&ptlist, dd.l[i]);
    }
    PL polist = LIT(-wid * 0.5, -hei, wid * 0.5, -hei, wid * 0.5, 0, mid, per, -wid * 0.5, 0);
    if (!b.tra) poly(polist, xoff, yoff, WHITE, COL_NONE, 0);
    for (int i = 0; i < ptlist.n; i++) {
        StrokeArgs s = stroke_args();
        s.col = rgba(100, 100, 100, 0.4); s.has_col = 1; s.noi = 1; s.wid = wei; s.fun = fn_one;
        stroke(pl_off(ptlist.l[i], xoff, yoff), &s);
    }
}

// ------------------------------------------------------------------ rail ----
static void rail(double xoff, double yoff, double seed, double hei, double wid, double rot,
                 double per, double seg, double wei, int tra, int fro) {
    double mid = -wid * 0.5 + wid * rot;
    double bmid = -wid * 0.5 + wid * (1 - rot);
    PLL ptlist = pll_new();
    Pt L0 = {-wid * 0.5, 0}, R0 = {wid * 0.5, 0}, LH = {-wid * 0.5, -hei}, RH = {wid * 0.5, -hei};
    if (fro) {
        pll_push(&ptlist, div2(L0, (Pt){mid, per}, seg));
        pll_push(&ptlist, div2((Pt){mid, per}, R0, seg));
    }
    if (tra) {
        pll_push(&ptlist, div2(L0, (Pt){bmid, -per}, seg));
        pll_push(&ptlist, div2((Pt){bmid, -per}, R0, seg));
    }
    if (fro) {
        pll_push(&ptlist, div2(LH, (Pt){mid, -hei + per}, seg));
        pll_push(&ptlist, div2((Pt){mid, -hei + per}, RH, seg));
    }
    if (tra) {
        pll_push(&ptlist, div2(LH, (Pt){bmid, -hei - per}, seg));
        pll_push(&ptlist, div2((Pt){bmid, -hei - per}, RH, seg));
    }
    if (tra) {
        int open = (int)floor(rnd() * ptlist.n);
        ptlist.l[open] = pl_slice(ptlist.l[open], 0, -1);
        int k = (open + ptlist.n) % ptlist.n;
        ptlist.l[k] = pl_slice(ptlist.l[k], 0, -1);
    }
    for (int i = 0; i < ptlist.n / 2.0; i++) {
        for (int j = 0; j < ptlist.l[i].n; j++) {
            int k = (ptlist.n / 2 + i) % ptlist.n;
            ptlist.l[i].p[j].y += (noise3(i, j * 0.5, seed) - 0.5) * hei;
            int jj = j % ptlist.l[k].n;
            ptlist.l[k].p[jj].y += (noise3(i + 0.5, j * 0.5, seed) - 0.5) * hei;
            PL two = pl_new();
            pl_push(&two, ptlist.l[i].p[j].x, ptlist.l[i].p[j].y);
            pl_push(&two, ptlist.l[k].p[jj].x, ptlist.l[k].p[jj].y);
            PL ln = div_pl(two, 2);
            ln.p[0].x += (rnd() - 0.5) * hei * 0.5;
            poly(ln, xoff, yoff, COL_NONE, rgba(100, 100, 100, 0.5), 2);
        }
    }
    for (int i = 0; i < ptlist.n; i++) {
        StrokeArgs s = stroke_args();
        s.col = rgba(100, 100, 100, 0.5); s.has_col = 1; s.noi = 0.5; s.wid = wei; s.fun = fn_one;
        stroke(pl_off(ptlist.l[i], xoff, yoff), &s);
    }
}

// ------------------------------------------------------------------ roof ----
static void roof(double xoff, double yoff, double hei, double wid, double rot, double per,
                 double cor, double wei, int pla_on, const char *pla_txt) {
    int fl = rot < 0.5;
    double rrot = rot < 0.5 ? 1 - rot : rot;
    double mid = -wid * 0.5 + wid * rrot;
    double quat = (mid + wid * 0.5) * 0.5 - mid;

    PLL ptlist = pll_new();
#define OPF(pl) (fl ? flip0(pl) : (pl))
    pll_push(&ptlist, div_pl(OPF(LIT(-wid * 0.5 + quat, -hei - per / 2, -wid * 0.5 + quat * 0.5, -hei / 2 - per / 4, -wid * 0.5 - cor, 0)), 5));
    pll_push(&ptlist, div_pl(OPF(LIT(mid + quat, -hei, (mid + quat + wid * 0.5) / 2, -hei / 2, wid * 0.5 + cor, 0)), 5));
    pll_push(&ptlist, div_pl(OPF(LIT(mid + quat, -hei, mid + quat / 2, -hei / 2 + per / 2, mid + cor, per)), 5));
    pll_push(&ptlist, div_pl(OPF(LIT(-wid * 0.5 - cor, 0, mid + cor, per)), 5));
    pll_push(&ptlist, div_pl(OPF(LIT(wid * 0.5 + cor, 0, mid + cor, per)), 5));
    pll_push(&ptlist, div_pl(OPF(LIT(-wid * 0.5 + quat, -hei - per / 2, mid + quat, -hei)), 5));

    PL polist = OPF(LIT(-wid * 0.5, 0, -wid * 0.5 + quat, -hei - per / 2, mid + quat, -hei, wid * 0.5, 0, mid, per));
    poly(polist, xoff, yoff, WHITE, COL_NONE, 0);
    for (int i = 0; i < ptlist.n; i++) {
        StrokeArgs s = stroke_args();
        s.col = rgba(100, 100, 100, 0.4); s.has_col = 1; s.noi = 1; s.wid = wei; s.fun = fn_one;
        stroke(pl_off(ptlist.l[i], xoff, yoff), &s);
    }
    if (pla_on) {
        PL pp = OPF(LIT(mid + quat / 2, -hei / 2 + per / 2, -wid * 0.5 + quat * 0.5, -hei / 2 - per / 4));
        if (pp.p[0].x > pp.p[1].x) { Pt t = pp.p[0]; pp.p[0] = pp.p[1]; pp.p[1] = t; }
        Pt mp = midPt(pp.p, 2);
        double a = atan2(pp.p[1].y - pp.p[0].y, pp.p[1].x - pp.p[0].x);
        double adeg = (a * 180) / M_PI;
        emit_text(mp.x + xoff, mp.y + yoff, hei * 0.6, adeg, rgba(100, 100, 100, 0.9), pla_txt);
    }
#undef OPF
}

// -------------------------------------------------------------- pagroof ----
static void pagroof(double xoff, double yoff, double hei, double wid, double per, double cor,
                    int sid, double wei) {
    PLL ptlist = pll_new();
    PL polist = pl_new();
    pl_push(&polist, 0, -hei);
    for (int i = 0; i < sid; i++) {
        double fx = wid * ((i * 1.0) / (sid - 1) - 0.5);
        double fy = per * (1 - fabs((i * 1.0) / (sid - 1) - 0.5) * 2);
        double fxx = (wid + cor) * ((i * 1.0) / (sid - 1) - 0.5);
        if (i > 0) {
            PL two = pl_new();
            PL prev = ptlist.l[ptlist.n - 1];
            pl_push(&two, prev.p[2].x, prev.p[2].y);
            pl_push(&two, fxx, fy);
            pll_push(&ptlist, two);
        }
        pll_push(&ptlist, LIT(0, -hei, fx * 0.5, (-hei + fy) * 0.5, fxx, fy));
        pl_push(&polist, fxx, fy);
    }
    poly(polist, xoff, yoff, WHITE, COL_NONE, 0);
    for (int i = 0; i < ptlist.n; i++) {
        StrokeArgs s = stroke_args();
        s.col = rgba(100, 100, 100, 0.4); s.has_col = 1; s.noi = 1; s.wid = wei; s.fun = fn_one;
        stroke(pl_off(div_pl(ptlist.l[i], 5), xoff, yoff), &s);
    }
}

// -------------------------------------------------------------------- Man ---
static PL tranpoly(Pt p0, Pt p1, PL ptlist) {
    PL plist = pl_new();
    for (int i = 0; i < ptlist.n; i++) pl_push(&plist, -ptlist.p[i].x, ptlist.p[i].y);
    double ang = atan2(p1.y - p0.y, p1.x - p0.x) - M_PI / 2;
    double scl = distance(p0, p1);
    PL q = pl_new();
    Pt zero = {0, 0};
    for (int i = 0; i < plist.n; i++) {
        double d = distance(plist.p[i], zero);
        double a = atan2(plist.p[i].y, plist.p[i].x);
        pl_push(&q, p0.x + d * scl * cos(ang + a), p0.y + d * scl * sin(ang + a));
    }
    return q;
}
static PL flipper(PL p) {
    PL q = pl_new();
    for (int i = 0; i < p.n; i++) pl_push(&q, -p.p[i].x, p.p[i].y);
    return q;
}

static void hat01(Pt p0, Pt p1, int fli) {
    double seed = rnd();
    PL a = LIT(-0.3, 0.5, 0.3, 0.8, 0.2, 1, 0, 1.1, -0.3, 1.15, -0.55, 1, -0.65, 0.5);
    poly(tranpoly(p0, p1, fli ? flipper(a) : a), 0, 0, rgba(100, 100, 100, 0.8), rgba(100, 100, 100, 0.8), 0);
    PL q = pl_new();
    for (int i = 0; i < 10; i++) pl_push(&q, -0.3 - noise3(i * 0.2, seed, 0) * i * 0.1, 0.5 - i * 0.3);
    poly(tranpoly(p0, p1, fli ? flipper(q) : q), 0, 0, rgba(0, 0, 0, 0), rgba(100, 100, 100, 0.8), 1);
}
static void hat02(Pt p0, Pt p1, int fli) {
    (void)rnd();   // var seed = Math.random()
    PL a = LIT(-0.3, 0.5, -1.1, 0.5, -1.2, 0.6, -1.1, 0.7, -0.3, 0.8, 0.3, 0.8, 1.0, 0.7, 1.3, 0.6, 1.2, 0.5, 0.3, 0.5);
    poly(tranpoly(p0, p1, fli ? flipper(a) : a), 0, 0, rgba(100, 100, 100, 0.8), rgba(100, 100, 100, 0.8), 0);
}
static void stick01(Pt p0, Pt p1, int fli) {
    double seed = rnd();
    PL q = pl_new();
    int l = 12;
    for (int i = 0; i < l; i++)
        pl_push(&q, -noise3(i * 0.1, seed, 0) * 0.1 * sin(((double)i / l) * M_PI) * 5, 0 + i * 0.3);
    poly(tranpoly(p0, p1, fli ? flipper(q) : q), 0, 0, rgba(0, 0, 0, 0), rgba(100, 100, 100, 0.5), 1);
}

typedef struct { PL a, b; } Pair;
static Pair expand(PL pt, double (*wfun)(double, double), double sca) {
    PL v0 = pl_new(), v1 = pl_new();
    (void)rnd();   // var n0 = Math.random() * 10 (se consume aunque no se use)
    for (int i = 1; i < pt.n - 1; i++) {
        double w = wfun((double)i / pt.n, sca);
        double a1 = atan2(pt.p[i].y - pt.p[i - 1].y, pt.p[i].x - pt.p[i - 1].x);
        double a2 = atan2(pt.p[i].y - pt.p[i + 1].y, pt.p[i].x - pt.p[i + 1].x);
        double a = (a1 + a2) / 2;
        if (a < a2) a += M_PI;
        pl_push(&v0, pt.p[i].x + w * cos(a), pt.p[i].y + w * sin(a));
        pl_push(&v1, pt.p[i].x - w * cos(a), pt.p[i].y - w * sin(a));
    }
    int l = pt.n - 1;
    double a0 = atan2(pt.p[1].y - pt.p[0].y, pt.p[1].x - pt.p[0].x) - M_PI / 2;
    double a1 = atan2(pt.p[l].y - pt.p[l - 1].y, pt.p[l].x - pt.p[l - 1].x) - M_PI / 2;
    double w0 = wfun(0, sca), w1 = wfun(1, sca);
    pl_unshift(&v0, pt.p[0].x + w0 * cos(a0), pt.p[0].y + w0 * sin(a0));
    pl_unshift(&v1, pt.p[0].x - w0 * cos(a0), pt.p[0].y - w0 * sin(a0));
    pl_push(&v0, pt.p[l].x + w1 * cos(a1), pt.p[l].y + w1 * sin(a1));
    pl_push(&v1, pt.p[l].x - w1 * cos(a1), pt.p[l].y - w1 * sin(a1));
    Pair r = {v0, v1};
    return r;
}

static double fsleeve(double x, double sca) {
    return sca * 8 * (sin(0.5 * x * M_PI) * pow(sin(x * M_PI), 0.1) + (1 - x) * 0.4);
}
static double fbody(double x, double sca) {
    return sca * 11 * (sin(0.5 * x * M_PI) * pow(sin(x * M_PI), 0.1) + (1 - x) * 0.5);
}
static double fhead(double x, double sca) {
    double d = x - 0.5;
    return sca * 7 * pow(0.25 - d * d, 0.3);
}

typedef struct { double xoff, yoff; int fli; } Glob;
static PL to_global(Glob g, PL p) {
    PL q = pl_new();
    for (int i = 0; i < p.n; i++) pl_push(&q, (g.fli ? -1 : 1) * p.p[i].x + g.xoff, p.p[i].y + g.yoff);
    return q;
}
static Pt to_global1(Glob g, Pt p) { return (Pt){(g.fli ? -1 : 1) * p.x + g.xoff, p.y + g.yoff}; }

static void cloth(Glob g, PL plist, double (*fun)(double, double), double sca) {
    PL tlist = bezmh(plist, 2);
    Pair e = expand(tlist, fun, sca);
    PL t1 = e.a, t2 = e.b;
    pl_reverse(&t2);
    poly(to_global(g, pl_cat(t1, t2)), 0, 0, WHITE, WHITE, 0);
    StrokeArgs s = stroke_args();
    s.wid = 1; s.col = rgba(100, 100, 100, 0.5); s.has_col = 1;
    stroke(to_global(g, t1), &s);
    StrokeArgs s2 = stroke_args();
    s2.wid = 1; s2.col = rgba(100, 100, 100, 0.6); s2.has_col = 1;
    stroke(to_global(g, t2), &s2);
}

// hat: 1 = hat01, 2 = hat02 ; ite: 0 = ninguno, 1 = stick01
void man(double xoff, double yoff, double sca, int hat, int ite, int fli, const double *lenarg) {
    double ang[9];
    ang[0] = 0;
    ang[1] = -M_PI / 2;
    ang[2] = normRand(0, 0);
    ang[3] = (M_PI / 4) * rnd();
    ang[4] = ((M_PI * 3) / 4) * rnd();
    ang[5] = (M_PI * 3) / 4;
    ang[6] = -M_PI / 4;
    ang[7] = (-M_PI * 3) / 4 - (M_PI / 4) * rnd();
    ang[8] = -M_PI / 4;
    static const double len_def[9] = {0, 30, 20, 30, 30, 30, 30, 30, 30};
    const double *l0 = lenarg ? lenarg : len_def;
    double len[9];
    for (int i = 0; i < 9; i++) len[i] = l0[i] * sca;

    static const int par[9][4] = {{0}, {0, 1}, {0, 1, 2}, {0, 3}, {0, 3, 4}, {0, 1, 5}, {0, 1, 5, 6}, {0, 1, 7}, {0, 1, 7, 8}};
    static const int parn[9] = {1, 2, 3, 2, 3, 3, 4, 3, 4};
    Pt pts[9];
    for (int i = 0; i < 9; i++) {
        double px = 0, py = 0;
        for (int k = 0; k < parn[i]; k++) {
            int node = par[i][k];
            double rot = 0;
            for (int m = 0; m < parn[node]; m++) rot += ang[par[node][m]];
            px += len[node] * cos(rot);
            py += len[node] * sin(rot);
        }
        pts[i].x = px; pts[i].y = py;
    }
    yoff -= pts[4].y;
    Glob g = {xoff, yoff, fli};

    if (ite == 1) stick01(to_global1(g, pts[8]), to_global1(g, pts[6]), fli);

    cloth(g, LIT(pts[1].x, pts[1].y, pts[7].x, pts[7].y, pts[8].x, pts[8].y), fsleeve, sca);
    cloth(g, LIT(pts[1].x, pts[1].y, pts[0].x, pts[0].y, pts[3].x, pts[3].y, pts[4].x, pts[4].y), fbody, sca);
    cloth(g, LIT(pts[1].x, pts[1].y, pts[5].x, pts[5].y, pts[6].x, pts[6].y), fsleeve, sca);
    cloth(g, LIT(pts[1].x, pts[1].y, pts[2].x, pts[2].y), fhead, sca);

    PL hlist = bezmh(LIT(pts[1].x, pts[1].y, pts[2].x, pts[2].y), 2);
    Pair e = expand(hlist, fhead, sca);
    PL h1 = e.a, h2 = e.b;
    pl_splice(&h1, 0, (int)floor(h1.n * 0.1));
    pl_splice(&h2, 0, (int)floor(h2.n * 0.95));
    pl_reverse(&h2);
    poly(to_global(g, pl_cat(h1, h2)), 0, 0, rgba(100, 100, 100, 0.6), rgba(100, 100, 100, 0.6), 0);

    Pt t1 = to_global1(g, pts[1]), t2 = to_global1(g, pts[2]);
    if (hat == 2) hat02(t1, t2, fli); else hat01(t1, t2, fli);
}

// ------------------------------------------------------------------ arch01 --
static DecSpec dec_none(void) { DecSpec d = {0, 0, 0, 0, 0}; return d; }

void arch01(double xoff, double yoff, double seed, const ArchArgs *a) {
    double hei = ARG(a->hei, 70), wid = ARG(a->wid, 180), per = ARG(a->per, 5);
    double p = 0.4 + rnd() * 0.2;
    double h0 = hei * p;
    double h1 = hei * (1 - p);

    hut(xoff, yoff - hei, h0, wid);
    BoxArgs b = {h1, (wid * 2) / 3, 0.7, per, 3, 1, 0, dec_none()};
    box(xoff, yoff, b);

    double r1 = rnd();
    rail(xoff, yoff, seed, 10, wid, 0.7, per * 2, (double)(int)(3 + r1 * 3), 1, 1, 0);

    static const double mc[4] = {0, 1, 1, 2};
    double mcnt = randChoiceD(mc, 4);
    if (mcnt == 1) {
        double nx = normRand(-wid / 3, wid / 3);
        double rf = rnd();
        man(xoff + nx, yoff, 0.42, 1, 0, floor(2 * rf) == 0 ? 1 : 0, NULL);
    } else if (mcnt == 2) {
        double n1 = normRand(-wid / 4, -wid / 5);
        man(xoff + n1, yoff, 0.42, 1, 0, 0, NULL);
        double n2 = normRand(wid / 5, wid / 4);
        man(xoff + n2, yoff, 0.42, 1, 0, 1, NULL);
    }
    double r2 = rnd();
    rail(xoff, yoff, seed, 10, wid, 0.7, per * 2, (double)(int)(3 + r2 * 3), 1, 0, 1);
}

// ------------------------------------------------------------------ arch02 --
void arch02(double xoff, double yoff, double seed, const ArchArgs *a) {
    double hei = ARG(a->hei, 10), wid = ARG(a->wid, 50), rot = ARG(a->rot, 0.3), per = ARG(a->per, 5);
    int sto = (int)ARG(a->sto, 3), sty = (int)ARG(a->sty, 1);
    int rai = ISU(a->rai) ? 0 : a->rai != 0;
    static const int hsp0[4] = {0, 1, 1, 1}, hsp1[4] = {0, 5, 5, 4}, vsp0[4] = {0, 1, 1, 1}, vsp1[4] = {0, 2, 2, 3};
    double hoff = 0;
    for (int i = 0; i < sto; i++) {
        BoxArgs b = {hei, wid * pow(0.85, i), rot, per, 1.5, 0, 1, {sty, hsp0[sty], hsp1[sty], vsp0[sty], vsp1[sty]}};
        box(xoff, yoff - hoff, b);
        if (rai) rail(xoff, yoff - hoff, i * 0.2, hei / 2, wid * pow(0.85, i) * 1.1, rot, per, 4, 0.5, 0, 1);
        int pla = 0;
        if (sto == 1 && rnd() < 1.0 / 3) pla = 1;
        roof(xoff, yoff - hoff - hei, hei, wid * pow(0.9, i), rot, per, 5, 1.5, pla, "Pizza Hut");
        hoff += hei * 1.5;
    }
}

void arch03(double xoff, double yoff, double seed, const ArchArgs *a) {
    double hei = ARG(a->hei, 10), wid = ARG(a->wid, 50), rot = ARG(a->rot, 0.7), per = ARG(a->per, 5);
    int sto = (int)ARG(a->sto, 7);
    double hoff = 0;
    for (int i = 0; i < sto; i++) {
        BoxArgs b = {hei, wid * pow(0.85, i), rot, per / 2, 1.5, 0, 1, {1, 1, 4, 1, 2}};
        box(xoff, yoff - hoff, b);
        rail(xoff, yoff - hoff, i * 0.2, hei / 2, wid * pow(0.85, i) * 1.1, rot, per / 2, 5, 0.5, 0, 1);
        pagroof(xoff, yoff - hoff - hei, hei * 1.5, wid * pow(0.9, i), per, 10, 4, 1.5);
        hoff += hei * 1.5;
    }
    (void)seed;
}

void arch04(double xoff, double yoff, double seed, const ArchArgs *a) {
    double hei = ARG(a->hei, 15), wid = ARG(a->wid, 30), rot = ARG(a->rot, 0.7), per = ARG(a->per, 5);
    int sto = (int)ARG(a->sto, 2);
    double hoff = 0;
    for (int i = 0; i < sto; i++) {
        BoxArgs b = {hei, wid * pow(0.85, i), rot, per / 2, 1.5, 1, 1, dec_none()};
        box(xoff, yoff - hoff, b);
        rail(xoff, yoff - hoff, i * 0.2, hei / 3, wid * pow(0.85, i) * 1.2, rot, per / 2, 3, 0.5, 1, 1);
        pagroof(xoff, yoff - hoff - hei, hei * 1, wid * pow(0.9, i), per, 10, 4, 1.5);
        hoff += hei * 1.2;
    }
    (void)seed;
}

// ------------------------------------------------------------------ boat01 --
static double boat_fn_sin2pi(double x, void *c) { (void)c; return sin(x * M_PI * 2); }

void boat01(double xoff, double yoff, double seed, const ArchArgs *a) {
    double len = ARG(a->len, 120), sca = ARG(a->sca, 1);
    int fli = ISU(a->fli) ? 0 : a->fli != 0;
    (void)seed;
    double dir = fli ? -1 : 1;
    static const double ml[9] = {0, 30, 20, 30, 10, 30, 30, 30, 30};
    man(xoff + 20 * sca * dir, yoff, 0.5 * sca, 2, 1, !fli, ml);

    PL p1 = pl_new(), p2 = pl_new();
    for (double i = 0; i < len * sca; i += 5 * sca) {
        double x1 = i / len;
        pl_push(&p1, i * dir, sqrt(sin(x1 * M_PI)) * 7 * sca);
        pl_push(&p2, i * dir, sqrt(sin(x1 * M_PI)) * 10 * sca);
    }
    pl_reverse(&p2);
    PL plist = pl_cat(p1, p2);
    poly(plist, xoff, yoff, WHITE, WHITE, 0);
    StrokeArgs s = stroke_args();
    s.wid = 1; s.fun = boat_fn_sin2pi; s.col = rgba(100, 100, 100, 0.4); s.has_col = 1;
    stroke(pl_off(plist, xoff, yoff), &s);
}

// ------------------------------------------------------ transmissionTower01 -
static double fn_half(double x, void *c) { (void)x; (void)c; return 0.5; }

static void quickstroke(double xoff, double yoff, PL pl) {
    StrokeArgs s = stroke_args();
    s.wid = 1; s.fun = fn_half; s.col = rgba(100, 100, 100, 0.4); s.has_col = 1;
    stroke(pl_off(div_pl(pl, 5), xoff, yoff), &s);
}

void transmissionTower01(double xoff, double yoff, double seed, const ArchArgs *a) {
    double hei = ARG(a->hei, 100), wid = ARG(a->wid, 20);
    (void)seed;
    Pt p00 = {-wid * 0.05, -hei}, p01 = {wid * 0.05, -hei};
    Pt p10 = {-wid * 0.1, -hei * 0.9}, p11 = {wid * 0.1, -hei * 0.9};
    Pt p20 = {-wid * 0.2, -hei * 0.5}, p21 = {wid * 0.2, -hei * 0.5};
    Pt p30 = {-wid * 0.5, 0}, p31 = {wid * 0.5, 0};
    static const double bch[3][2] = {{0.7, -0.85}, {1, -0.675}, {0.7, -0.5}};

    for (int i = 0; i < 3; i++) {
        quickstroke(xoff, yoff, LIT(-bch[i][0] * wid, bch[i][1] * hei, bch[i][0] * wid, bch[i][1] * hei));
        quickstroke(xoff, yoff, LIT(-bch[i][0] * wid, bch[i][1] * hei, 0, (bch[i][1] - 0.05) * hei));
        quickstroke(xoff, yoff, LIT(bch[i][0] * wid, bch[i][1] * hei, 0, (bch[i][1] - 0.05) * hei));
        quickstroke(xoff, yoff, LIT(-bch[i][0] * wid, bch[i][1] * hei, -bch[i][0] * wid, (bch[i][1] + 0.1) * hei));
        quickstroke(xoff, yoff, LIT(bch[i][0] * wid, bch[i][1] * hei, bch[i][0] * wid, (bch[i][1] + 0.1) * hei));
    }
    PL l10 = div_pl(LIT(p00.x, p00.y, p10.x, p10.y, p20.x, p20.y, p30.x, p30.y), 5);
    PL l11 = div_pl(LIT(p01.x, p01.y, p11.x, p11.y, p21.x, p21.y, p31.x, p31.y), 5);
    for (int i = 0; i < l10.n - 1; i++) {
        quickstroke(xoff, yoff, LIT(l10.p[i].x, l10.p[i].y, l11.p[i + 1].x, l11.p[i + 1].y));
        quickstroke(xoff, yoff, LIT(l11.p[i].x, l11.p[i].y, l10.p[i + 1].x, l10.p[i + 1].y));
    }
    quickstroke(xoff, yoff, LIT(p00.x, p00.y, p01.x, p01.y));
    quickstroke(xoff, yoff, LIT(p10.x, p10.y, p11.x, p11.y));
    quickstroke(xoff, yoff, LIT(p20.x, p20.y, p21.x, p21.y));
    quickstroke(xoff, yoff, LIT(p00.x, p00.y, p10.x, p10.y, p20.x, p20.y, p30.x, p30.y));
    quickstroke(xoff, yoff, LIT(p01.x, p01.y, p11.x, p11.y, p21.x, p21.y, p31.x, p31.y));
}

// ------------------------------------------------------------------- water --
void water(double xoff, double yoff, double seed, double hei, double len, double clu) {
    (void)seed;
    if (ISU(hei)) hei = 2;
    if (ISU(len)) len = 800;
    if (ISU(clu)) clu = 10;
    PLL ptlist = pll_new();
    double yk = 0;
    for (int i = 0; i < clu; i++) {
        PL row = pl_new();
        double xk = (rnd() - 0.5) * (len / 8);
        yk += rnd() * 5;
        double lk = len / 4 + rnd() * (len / 4);
        double reso = 5;
        for (double j = -lk; j < lk; j += reso)
            pl_push(&row, j + xk, sin(j * 0.2) * hei * noise3(j * 0.1, 0, 0) - 20 + yk);
        pll_push(&ptlist, row);
    }
    for (int j = 1; j < ptlist.n; j += 1) {
        double r = rnd();
        StrokeArgs s = stroke_args();
        s.col = rgbaq(100, 100, 100, 0.3 + r * 0.3, 3); s.has_col = 1; s.wid = 1;
        stroke(pl_off(ptlist.l[j], xoff, yoff), &s);
    }
}
