// Elementos japoneses de nippon. Todo con la misma tinta del original; el color es poco,
// apagado y siempre difuminado hacia los bordes (soft_tint).
#include "gen.h"

#define LIT(...) pl_lit((int)(sizeof((double[]){__VA_ARGS__}) / sizeof(double) / 2), (double[]){__VA_ARGS__})
static const Col WH = {255, 255, 255, 2, 1};
static Col ink(double a) { return rgba(100, 100, 100, a); }

static void sk(PL p, Col c, double w, double noi) {        // trazo de pincel de ancho parejo
    StrokeArgs s = stroke_args();
    s.col = c; s.has_col = 1; s.wid = w; s.fun = fn_one; s.noi = noi;
    stroke(p, &s);
}
static void sk2(double x0, double y0, double x1, double y1, Col c, double w) {
    sk(div_pl(LIT(x0, y0, x1, y1), 6), c, w, 0.3);
}
static PL ellipse(double cx, double cy, double rx, double ry, int n) {
    PL e = pl_new();
    for (int i = 0; i < n; i++) {
        double a = 2 * M_PI * i / n;
        pl_push(&e, cx + cos(a) * rx, cy + sin(a) * ry);
    }
    return e;
}

// Color difuminado: capas cada vez mas chicas hacia el centro, el borde queda sin color.
void soft_tint(PL p, double xo, double yo, int r, int g, int b, double alpha, int layers) {
    if (p.n < 3) return;
    Pt c = midPt(p.p, p.n);
    for (int k = 1; k <= layers; k++) {
        double s = 1 - (double)k / (layers + 1);
        PL q = pl_new();
        for (int i = 0; i < p.n; i++) pl_push(&q, c.x + (p.p[i].x - c.x) * s, c.y + (p.p[i].y - c.y) * s);
        poly(q, xo, yo, rgba(r, g, b, alpha / layers), COL_NONE, 0);
    }
}

// ------------------------------------------------------------------ bambu ---
void bamboo(double x, double y, double hei, int n) {
    Col cane = rgba(92, 108, 84, 0.55), leaf = rgba(88, 104, 80, 0.45);
    for (int i = 0; i < n; i++) {
        double cx = x + (rnd() - 0.5) * n * 7, h = hei * (0.7 + 0.5 * rnd()), lean = (rnd() - 0.5) * h * 0.15;
        PL c = pl_new();
        for (int k = 0; k <= 10; k++) { double t = k / 10.0; pl_push(&c, cx + lean * t * t, y - h * t); }
        sk(c, cane, 1.3, 0.2);
        for (int k = 1; k < 6; k++) {                          // nudos
            double t = k / 6.0, nx = cx + lean * t * t, ny = y - h * t;
            sk2(nx - 1.6, ny, nx + 1.6, ny, ink(0.5), 0.6);
        }
        int nl = 5 + (int)(rnd() * 5);
        for (int k = 0; k < nl; k++) {                         // hojas finas colgando
            double t = 0.45 + rnd() * 0.55, nx = cx + lean * t * t, ny = y - h * t;
            double side = rnd() < 0.5 ? -1 : 1;
            BlobArgs b = blob_args();
            b.len = 9 + rnd() * 6; b.wid = 2.4; b.ang = side > 0 ? 0.35 + rnd() * 0.4 : M_PI - 0.35 - rnd() * 0.4;
            b.col = leaf; b.has_col = 1;
            blob(nx + side * 5, ny + 2, &b);
        }
    }
}

// ---------------------------------------------------------- pino japones ---
// Tronco sinuoso y copas planas en capas (kuromatsu).
void jpine(double x, double y, double hei) {
    double sway = (rnd() - 0.5) * hei * 0.35;
    PL tr = pl_new();
    for (int k = 0; k <= 12; k++) {
        double t = k / 12.0;
        pl_push(&tr, x + sway * sin(t * M_PI * 0.9) + (rnd() - 0.5) * 1.2, y - hei * t);
    }
    PL trunk = pl_copy(tr);
    StrokeArgs s = stroke_args();
    s.col = ink(0.6); s.has_col = 1; s.wid = 2.6; s.noi = 0.6;
    stroke(trunk, &s);
    int nb = 3 + (int)(rnd() * 3);
    for (int i = 0; i < nb; i++) {
        double t = 0.45 + 0.55 * i / (nb - 0.5 > 0 ? nb - 0.5 : 1);
        if (t > 1) t = 1;
        int k = (int)(t * 12);
        Pt b0 = tr.p[k];
        double dir = (i % 2 ? 1 : -1) * (rnd() < 0.8 ? 1 : -1);
        double bl = hei * (0.25 + rnd() * 0.25) * (1.15 - t * 0.6);
        Pt b1 = {b0.x + dir * bl, b0.y - bl * 0.25};
        sk2(b0.x, b0.y, b1.x, b1.y, ink(0.55), 1.4);
        int pads = 2 + (int)(rnd() * 2);
        for (int p = 0; p < pads; p++) {                       // copa plana
            BlobArgs bb = blob_args();
            bb.len = bl * (0.7 + rnd() * 0.5) + 10; bb.wid = 5 + rnd() * 3; bb.ang = (rnd() - 0.5) * 0.15;
            bb.col = rgba(90, 96, 88, 0.5 + rnd() * 0.15); bb.has_col = 1;
            blob(b1.x - dir * p * 4, b1.y - p * 3.5, &bb);
        }
    }
}

// ------------------------------------------------------- piedras y faroles ---
static void stone_box(double x0, double y0, double w, double h) {
    PL r = LIT(x0, y0, x0 + w, y0, x0 + w, y0 + h, x0, y0 + h);
    poly(r, 0, 0, rgb3(214, 212, 205), COL_NONE, 0);
    PL o = LIT(x0, y0 + h, x0, y0, x0 + w, y0, x0 + w, y0 + h);
    sk(div_pl(o, 4), ink(0.55), 0.9, 0.4);
}
void gravestone(double x, double y, double s) {           // haka: base, escalon y pilar
    stone_box(x - 8 * s, y - 3 * s, 16 * s, 3 * s);
    stone_box(x - 6 * s, y - 6 * s, 12 * s, 3 * s);
    PL p = LIT(x - 3.5 * s, y - 6 * s, x - 3.5 * s, y - 21 * s, x, y - 23 * s, x + 3.5 * s, y - 21 * s, x + 3.5 * s, y - 6 * s);
    poly(p, 0, 0, rgb3(208, 206, 199), COL_NONE, 0);
    sk(div_pl(p, 3), ink(0.6), 0.9, 0.4);
    sk2(x - 1 * s, y - 18 * s, x - 1 * s, y - 9 * s, ink(0.35), 0.6);   // inscripcion
    sk2(x + 1 * s, y - 18 * s, x + 1 * s, y - 11 * s, ink(0.3), 0.6);
    if (rnd() < 0.6) for (int i = 0; i < 2; i++)                          // sotoba (tablillas)
        sk2(x + (5 + i * 2) * s, y - 4 * s, x + (6 + i * 2) * s, y - 26 * s, ink(0.4), 0.7);
}
void toro(double x, double y, double s) {                 // farol de piedra
    stone_box(x - 6 * s, y - 3 * s, 12 * s, 3 * s);
    stone_box(x - 2 * s, y - 14 * s, 4 * s, 11 * s);
    stone_box(x - 5 * s, y - 16 * s, 10 * s, 2 * s);
    stone_box(x - 4 * s, y - 22 * s, 8 * s, 6 * s);
    poly(LIT(x - 1.5 * s, y - 21 * s, x + 1.5 * s, y - 21 * s, x + 1.5 * s, y - 17 * s, x - 1.5 * s, y - 17 * s), 0, 0, ink(0.6), COL_NONE, 0);
    PL roof = LIT(x - 8 * s, y - 21 * s, x - 3 * s, y - 25 * s, x, y - 27 * s, x + 3 * s, y - 25 * s, x + 8 * s, y - 21 * s);
    poly(roof, 0, 0, rgb3(205, 203, 196), COL_NONE, 0);
    sk(div_pl(roof, 3), ink(0.6), 1.0, 0.4);
    sk2(x, y - 27 * s, x, y - 30 * s, ink(0.6), 1.2);
}
void shrine_group(double x, double y) {                  // torii + faroles o lapidas
    torii(x, y, 0);
    if (rnd() < 0.6) { toro(x - 22, y + 2, 0.8); toro(x + 22, y + 2, 0.8); }
    if (rnd() < 0.35) for (int i = 0; i < 3 + (int)(rnd() * 3); i++) gravestone(x + 30 + i * 13, y + (i % 2) * 3, 0.7);
}

// ------------------------------------------------------------- agua y koi ---
void water_wash(double x, double y, double len) {         // celeste muy difuminado
    soft_tint(ellipse(x, y, len * 0.55, 18, 40), 0, 0, 150, 196, 222, 0.22, 7);
}
void koi(double x, double y, double ang, double s) {
    int white = rnd() < 0.35;
    BlobArgs b = blob_args();
    b.len = 14 * s; b.wid = 4.5 * s; b.ang = ang; b.noi = 0.3;
    b.col = white ? rgba(214, 210, 202, 0.55) : rgba(206, 118, 74, 0.42); b.has_col = 1;
    blob(x, y, &b);
    if (white) {                                           // mancha naranja (kohaku)
        BlobArgs m = blob_args();
        m.len = 5 * s; m.wid = 3 * s; m.ang = ang; m.col = rgba(206, 118, 74, 0.45); m.has_col = 1;
        blob(x + cos(ang) * 2 * s, y + sin(ang) * 2 * s, &m);
    }
    BlobArgs t = blob_args();                              // cola
    t.len = 6 * s; t.wid = 4 * s; t.ang = ang; t.col = rgba(180, 120, 90, 0.3); t.has_col = 1;
    blob(x - cos(ang) * 9 * s, y - sin(ang) * 9 * s, &t);
}
void koi_pond(double x, double y) {
    water_wash(x, y, 160);
    int n = 2 + (int)(rnd() * 4);
    for (int i = 0; i < n; i++) koi(x + (rnd() - 0.5) * 90, y + (rnd() - 0.5) * 14, rnd() * 2 * M_PI, 0.8 + rnd() * 0.4);
    for (int i = 0; i < 3; i++) {                           // ondas
        double cx = x + (rnd() - 0.5) * 100;
        PL w = ellipse(cx, y + (rnd() - 0.5) * 10, 6 + i * 4, 1.5 + i, 18);
        pl_push(&w, w.p[0].x, w.p[0].y);
        poly(w, 0, 0, COL_NONE, rgba(110, 150, 175, 0.25), 0.8);
    }
}

// ----------------------------------------------------------------- puente ---
void bridge(double x, double y, double len) {             // taiko-bashi: arco de madera
    double h = len * 0.22;
    PL top = pl_new(), bot = pl_new();
    for (int i = 0; i <= 24; i++) {
        double t = i / 12.0 - 1;
        double yy = y - h * (1 - t * t);
        pl_push(&top, x + t * len / 2, yy);
        pl_push(&bot, x + t * len / 2, yy + 4 + 3 * (1 - fabs(t)));
    }
    PL deck = pl_copy(top);
    for (int i = bot.n - 1; i >= 0; i--) pl_push(&deck, bot.p[i].x, bot.p[i].y);
    poly(deck, 0, 0, WH, COL_NONE, 0);
    soft_tint(deck, 0, 0, 176, 86, 64, 0.45, 3);
    sk(top, ink(0.65), 1.4, 0.3);
    sk(bot, ink(0.45), 1.0, 0.3);
    PL rail = pl_new();
    for (int i = 0; i <= 24; i++) pl_push(&rail, top.p[i].x, top.p[i].y - 7);
    sk(rail, rgba(160, 96, 78, 0.45), 1.3, 0.3);
    for (int i = 0; i <= 24; i += 4) sk2(top.p[i].x, top.p[i].y, rail.p[i].x, rail.p[i].y - 1.5, rgba(150, 90, 72, 0.45), 1.2);
    for (int k = -1; k <= 1; k += 2) sk2(x + k * len * 0.32, y - h * 0.55, x + k * len * 0.34, y + 8, ink(0.45), 1.6);   // pilares
}

// ------------------------------------------------------- samurai y duelo ---
void samurai(double x, double y, double sca, int fli) { man(x, y, sca, 1, 2, fli, NULL); }
void duel(double x, double y, double sca) {
    double gap = 22 * sca;
    samurai(x - gap / 2, y, sca, 0);
    samurai(x + gap / 2, y, sca, 1);
    for (int i = 0; i < 4; i++)                               // polvo / viento
        sk2(x - 30 * sca + rnd() * 10, y + 1 + i, x + 30 * sca - rnd() * 10, y + 1 + i, ink(0.12), 0.6);
}

// ------------------------------------------------------------------ aldea ---
void village(double x, double y) {
    int nh = 3 + (int)(rnd() * 4);
    if (rnd() < 0.5) shrine_group(x - 120, y + 6);
    for (int i = 0; i < nh; i++) {
        double hx = x + (i - nh / 2.0) * (55 + rnd() * 15), hy = y + (rnd() - 0.5) * 18;
        ArchArgs a = arch_args();
        a.wid = 40 + rnd() * 20; a.sto = 1; a.rot = rnd(); a.sty = 1 + (int)(rnd() * 3);
        arch02(hx, hy, rnd() * 10, &a);
        if (rnd() < 0.35) man(hx + 25, hy + 2, 0.36, 1, 0, rnd() < 0.5, NULL);
    }
    bamboo(x + nh * 34, y + 4, 70, 4 + (int)(rnd() * 4));
    if (rnd() < 0.4) samurai(x - 40, y + 10, 0.4, rnd() < 0.5);
    if (rnd() < 0.5) jpine(x - nh * 30, y + 6, 70 + rnd() * 40);
}

// ------------------------------------------------- barca japonesa (wasen) ---
void jboat(double x, double y, double sca, int fli) {
    double dir = fli ? -1 : 1, L = 120 * sca;
    static const double ml[9] = {0, 30, 20, 30, 10, 30, 30, 30, 30};
    man(x + 26 * sca * dir, y, 0.5 * sca, 2, 1, !fli, ml);
    PL top = pl_new(), bot = pl_new();
    for (int i = 0; i <= 20; i++) {
        double t = i / 20.0;
        pl_push(&top, x + t * L * dir, y - 9 * sca * pow(t, 4));          // proa levantada
        pl_push(&bot, x + t * L * dir, y + sqrt(sin(t * M_PI * 0.98 + 0.01)) * 7 * sca - 9 * sca * pow(t, 4) * 0.6);
    }
    PL hull = pl_copy(top);
    for (int i = bot.n - 1; i >= 0; i--) pl_push(&hull, bot.p[i].x, bot.p[i].y);
    poly(hull, 0, 0, WH, WH, 0);
    sk(top, ink(0.6), 1.1, 0.3);
    sk(bot, ink(0.4), 1.0, 0.3);
    if (rnd() < 0.5) {                                     // tomaya: techito de paja curvo
        PL r = pl_new();
        for (int i = 0; i <= 8; i++) { double t = i / 8.0; pl_push(&r, x + (0.45 + t * 0.25) * L * dir, y - 10 * sca - sin(t * M_PI) * 4 * sca); }
        sk(r, ink(0.5), 1.6, 0.5);
        sk2(r.p[0].x, r.p[0].y, r.p[0].x, y, ink(0.4), 0.8);
        sk2(r.p[8].x, r.p[8].y, r.p[8].x, y - 1, ink(0.4), 0.8);
    }
    sk2(x - 4 * dir * sca, y - 4 * sca, x - 22 * dir * sca, y + 10 * sca, ink(0.5), 1.0);   // remo (ro)
}

// --------------------------------------------------------- easter eggs ---
void mecha(double x, double y, double h) {               // silueta lejana, casi niebla
    Col f = rgba(150, 156, 168, 0.28), o = ink(0.3);
    double u = h / 10;
#define BOX(x0, y0, w, hh) do { PL _b = LIT(x + (x0) * u, y - (y0) * u, x + ((x0) + (w)) * u, y - (y0) * u, x + ((x0) + (w)) * u, y - ((y0) + (hh)) * u, x + (x0) * u, y - ((y0) + (hh)) * u); poly(_b, 0, 0, f, o, 0.8); } while (0)
    BOX(-1.6, 0, 1.2, 4.2); BOX(0.4, 0, 1.2, 4.2);            // piernas
    BOX(-1.8, 4.2, 3.6, 1.0);                                 // cadera
    BOX(-1.5, 5.2, 3.0, 2.6);                                 // torso
    BOX(-2.9, 7.0, 1.3, 1.2); BOX(1.6, 7.0, 1.3, 1.2);        // hombros
    BOX(-2.7, 3.6, 0.9, 3.4); BOX(1.8, 3.6, 0.9, 3.4);        // brazos
    BOX(-0.6, 7.8, 1.2, 1.1);                                 // cabeza
#undef BOX
    sk2(x, y - 8.9 * u, x - 0.9 * u, y - 9.8 * u, o, 0.8);    // antena en V
    sk2(x, y - 8.9 * u, x + 0.9 * u, y - 9.8 * u, o, 0.8);
}

static void statue(double x, double y, double s, int reach) {   // persona petrificada
    Col st = rgb3(206, 204, 196);
    PL body = LIT(x - 3 * s, y, x - 2.5 * s, y - 11 * s, x - 3.5 * s, y - 18 * s, x + 3.5 * s, y - 18 * s, x + 2.5 * s, y - 11 * s, x + 3 * s, y);
    poly(body, 0, 0, st, COL_NONE, 0);
    sk(div_pl(body, 3), ink(0.55), 0.9, 0.4);
    PL head = ellipse(x, y - 21 * s, 2.6 * s, 3 * s, 14);
    poly(head, 0, 0, st, ink(0.55), 0.9);
    if (reach) sk2(x + 3 * s, y - 16 * s, x + 9 * s, y - 15 * s, ink(0.5), 1.6 * s);   // brazo extendido
    for (int i = 0; i < 4; i++) {                              // grietas
        double cx = x + (rnd() - 0.5) * 5 * s, cy = y - (4 + rnd() * 18) * s;
        sk(LIT(cx, cy, cx + 1.5 * s, cy - 1.5 * s, cx + 0.5 * s, cy - 3 * s, cx + 2 * s, cy - 4.5 * s), ink(0.4), 0.5, 0.2);
    }
    for (int i = 0; i < 3; i++) {                              // musgo
        BlobArgs b = blob_args();
        b.len = 4 * s; b.wid = 1.6 * s; b.ang = rnd() * M_PI; b.col = rgba(110, 130, 90, 0.35); b.has_col = 1;
        blob(x + (rnd() - 0.5) * 5 * s, y - rnd() * 12 * s, &b);
    }
}
void stone_couple(double x, double y) {                  // el gran alcanforero y las dos estatuas
    TreeArgs a = tree_args();
    a.hei = 220;
    tree04(x, y, &a);
    statue(x - 14, y + 2, 1.1, 1);
    statue(x + 2, y + 3, 1.0, 0);
}

// ------------------------------------------------------------------- isla ---
// Tierra baja y plana para que aldeas, duelos y puentes no floten en el agua.
void islet(double x, double y, double w) {
    double h = 7 + w * 0.035;
    PL top = pl_new(), bot = pl_new();
    for (int i = 0; i <= 24; i++) {
        double t = i / 12.0 - 1;                           // -1 .. 1
        double edge = pow(1 - fabs(t), 0.35);              // flancos cortos, techo casi plano
        double nz = noise3(x * 0.01 + i * 0.3, y * 0.01, 7) - 0.5;
        pl_push(&top, x + t * w / 2, y - h * edge + nz * 3);
        pl_push(&bot, x + t * w / 2, y + 3 * edge);
    }
    PL land = pl_copy(top);
    for (int i = bot.n - 1; i >= 0; i--) pl_push(&land, bot.p[i].x, bot.p[i].y);
    poly(land, 0, 0, WH, COL_NONE, 0);
    StrokeArgs s = stroke_args();
    s.col = rgba(100, 100, 100, 0.35); s.has_col = 1; s.wid = 1.6; s.noi = 0.8;
    stroke(top, &s);
    for (int k = 0; k < (int)(w / 40) + 1; k++) {          // pasto/textura corta
        double tx = x + (rnd() - 0.5) * w * 0.8, ty = y - h * 0.5 + rnd() * h * 0.4;
        sk2(tx, ty, tx + 6 + rnd() * 8, ty - 0.5, ink(0.18), 0.7);
    }
    for (int k = -1; k <= 1; k += 2) {                     // piedritas en las puntas
        BlobArgs b = blob_args();
        b.len = 8 + rnd() * 6; b.wid = 4 + rnd() * 2; b.col = rgba(120, 120, 115, 0.35); b.has_col = 1;
        blob(x + k * w * 0.46, y - 1, &b);
    }
    for (int k = 0; k < 2; k++) {                          // agua alrededor
        double wy = y + 5 + k * 4, wl = w * (0.6 + 0.3 * k);
        sk(div_pl(LIT(x - wl / 2, wy, x + wl / 2, wy + 0.5), 10), rgba(92, 128, 152, 0.3), 0.9, 0.5);
    }
}

// Dos islas unidas por un puente (y algo encima de alguna).
void bridged_islets(double x, double y, double len) {
    double iw = 70 + rnd() * 40;
    double xl = x - len / 2 - iw * 0.38, xr = x + len / 2 + iw * 0.38;
    islet(xl, y + 6, iw);
    islet(xr, y + 6, iw);
    bridge(x, y + 2, len + iw * 0.3);
    if (rnd() < 0.6) jpine(xl - iw * 0.15, y + 2, 45 + rnd() * 30);
    if (rnd() < 0.5) toro(xr + iw * 0.18, y + 2, 0.8);
}
// -------------------------------------------------- cielo y mar (capa de fondo) ---
static const Col INDIGO_LINE = {74, 96, 138, 0, 0.45};

static PL arc(double cx, double cy, double rx, double ry, double a0, double a1, int n) {
    PL p = pl_new();
    for (int i = 0; i <= n; i++) {
        double a = a0 + (a1 - a0) * i / n;
        pl_push(&p, cx + cos(a) * rx, cy + sin(a) * ry);
    }
    return p;
}

// Nube estilizada (kumo): lomos redondos sobre una base plana, con arcos concentricos.
void kumo(double x, double y, double s) {
    int nb = 3 + (int)(rnd() * 3);
    double w = 0;
    double r[6], bx[6];
    for (int i = 0; i < nb; i++) { r[i] = (10 + rnd() * 10) * s; bx[i] = w; w += r[i] * 1.25; }
    double off = -w / 2;
    PL base = pl_new();
    pl_push(&base, x + off - 6 * s, y);
    for (int i = 0; i < nb; i++) {
        double cx = x + off + bx[i] + r[i], cy = y - r[i] * 0.15;
        PL d = arc(cx, cy, r[i], r[i] * 0.85, M_PI, 2 * M_PI, 16);
        pl_push(&d, cx + r[i], y); pl_push(&d, cx - r[i], y);
        poly(d, 0, 0, WH, COL_NONE, 0);
        soft_tint(d, 0, 0, 120, 140, 178, 0.22, 3);
        poly(arc(cx, cy, r[i], r[i] * 0.85, M_PI, 2 * M_PI, 16), 0, 0, COL_NONE, INDIGO_LINE, 1.1);
        for (int k = 1; k <= 2; k++)                       // arcos concentricos internos
            poly(arc(cx, cy + 1, r[i] * (1 - 0.3 * k), r[i] * 0.85 * (1 - 0.3 * k), M_PI * 1.1, M_PI * 1.9, 10),
                 0, 0, COL_NONE, rgba(74, 96, 138, 0.3), 0.8);
    }
    pl_push(&base, x - off + 6 * s, y);
    poly(base, 0, 0, COL_NONE, INDIGO_LINE, 1.0);
}

// Bruma en bandas (suyari-gasumi): barras horizontales redondeadas y escalonadas.
void kasumi(double x, double y, double len) {
    int nb = 2 + (int)(rnd() * 3);
    for (int i = 0; i < nb; i++) {
        double l = len * (0.5 + rnd() * 0.5), h = 7 + rnd() * 5;
        double bx = x + (rnd() - 0.5) * len * 0.4, by = y + i * (h + 2);
        PL b = pl_new();
        for (int k = 0; k <= 8; k++) { double a = M_PI / 2 + M_PI * k / 8; pl_push(&b, bx - l / 2 + cos(a) * h / 2, by + sin(a) * h / 2); }
        for (int k = 0; k <= 8; k++) { double a = -M_PI / 2 + M_PI * k / 8; pl_push(&b, bx + l / 2 + cos(a) * h / 2, by + sin(a) * h / 2); }
        poly(b, 0, 0, rgba(132, 150, 182, 0.22), COL_NONE, 0);
    }
}

// Ola estilo Hokusai, chica: cuerpo con lineas paralelas, cresta que se enrosca y garras de espuma.
void nami(double x, double y, double s) {
    double w = 60 * s, h = 28 * s;
    PL body = pl_new();
    for (int i = 0; i <= 20; i++) {                        // lomo: sube suave y cae enroscado
        double t = i / 20.0;
        pl_push(&body, x - w / 2 + t * w * 0.8, y - h * sin(t * M_PI * 0.55) * (0.6 + 0.4 * t));
    }
    double cx = x + w * 0.3, cy = y - h * 0.62, cr = h * 0.38;
    PL curl = arc(cx, cy, cr, cr, -M_PI * 0.6, M_PI * 0.9, 14);
    for (int i = 0; i < curl.n; i++) pl_push(&body, curl.p[i].x, curl.p[i].y);
    pl_push(&body, x + w / 2, y + 2);
    pl_push(&body, x - w / 2, y + 2);
    poly(body, 0, 0, WH, COL_NONE, 0);
    soft_tint(body, 0, 0, 92, 120, 168, 0.35, 4);
    PL crest = pl_new();
    for (int i = 0; i < body.n - 2; i++) pl_push(&crest, body.p[i].x, body.p[i].y);
    sk(crest, rgba(60, 84, 128, 0.6), 1.2, 0.3);
    for (int k = 1; k <= 3; k++) {                         // lineas paralelas dentro
        PL l = pl_new();
        for (int i = 0; i <= 14; i++) {
            double t = i / 14.0;
            pl_push(&l, x - w / 2 + k * 6 * s + t * w * 0.62, y - (h - k * 6 * s) * sin(t * M_PI * 0.55) * (0.6 + 0.4 * t) + k * 2 * s);
        }
        sk(l, rgba(74, 96, 138, 0.35), 0.8, 0.3);
    }
    for (int k = 0; k < 4; k++) {                          // garras de espuma en la cresta
        double a = -M_PI * 0.5 + k * 0.35;
        double fx = cx + cos(a) * cr, fy = cy + sin(a) * cr;
        sk(LIT(fx, fy, fx + 3 * s, fy - 3 * s, fx + 5 * s, fy - 1 * s), rgba(60, 84, 128, 0.5), 0.7, 0.2);
    }
    for (int k = 0; k < 5; k++) {                          // gotitas
        BlobArgs b = blob_args();
        b.len = 2 * s; b.wid = 2 * s; b.col = rgba(74, 96, 138, 0.35); b.has_col = 1;
        blob(cx + (rnd() - 0.2) * cr * 2, cy - cr - rnd() * 8 * s, &b);
    }
    sk(div_pl(LIT(x - w * 0.7, y + 4, x + w * 0.7, y + 4.5), 12), rgba(92, 128, 152, 0.3), 0.8, 0.5);
}