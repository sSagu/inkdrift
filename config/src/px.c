// px: lienzo, paleta y primitivas pixel-art (ver px.h).
#include "px.h"
#include <stdlib.h>
#include <string.h>

int px_canvas_init(PxCanvas *c, int w, int h) {
    c->px = (uint32_t *)calloc((size_t)w * h, 4);
    c->w = w; c->h = h;
    c->clip = pxr(0, 0, w, h);
    return c->px != NULL;
}
void px_canvas_free(PxCanvas *c) { free(c->px); c->px = NULL; }

PxRect px_set_clip(PxCanvas *c, PxRect r) {
    PxRect old = c->clip;
    int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
    int x1 = r.x + r.w > c->w ? c->w : r.x + r.w, y1 = r.y + r.h > c->h ? c->h : r.y + r.h;
    c->clip = pxr(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
    return old;
}
void px_reset_clip(PxCanvas *c) { c->clip = pxr(0, 0, c->w, c->h); }

void px_clear(PxCanvas *c, uint32_t col) { for (int i = 0; i < c->w * c->h; i++) c->px[i] = col; }

void px_pset(PxCanvas *c, int x, int y, uint32_t col) {
    if (!(col >> 24)) return;
    if (px_in(c->clip, x, y)) c->px[y * c->w + x] = col;
}
uint32_t px_get(const PxCanvas *c, int x, int y) {
    return (x >= 0 && y >= 0 && x < c->w && y < c->h) ? c->px[y * c->w + x] : 0;
}

void px_fill(PxCanvas *c, PxRect r, uint32_t col) {
    if (!(col >> 24)) return;
    int x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    if (x0 < c->clip.x) x0 = c->clip.x;
    if (y0 < c->clip.y) y0 = c->clip.y;
    if (x1 > c->clip.x + c->clip.w) x1 = c->clip.x + c->clip.w;
    if (y1 > c->clip.y + c->clip.h) y1 = c->clip.y + c->clip.h;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) c->px[y * c->w + x] = col;
}
void px_hline(PxCanvas *c, int x, int y, int w, uint32_t col) { px_fill(c, pxr(x, y, w, 1), col); }
void px_vline(PxCanvas *c, int x, int y, int h, uint32_t col) { px_fill(c, pxr(x, y, 1, h), col); }

void px_line(PxCanvas *c, int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        px_pset(c, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

void px_outline(PxCanvas *c, PxRect r, uint32_t col) {
    px_hline(c, r.x, r.y, r.w, col);
    px_hline(c, r.x, r.y + r.h - 1, r.w, col);
    px_vline(c, r.x, r.y + 1, r.h - 2, col);
    px_vline(c, r.x + r.w - 1, r.y + 1, r.h - 2, col);
}

// sangria de la fila i (0..h-1) de un octogono de chaflan k
static int cham_inset(int i, int h, int k) {
    int a = k - i, b = k - (h - 1 - i), m = a > b ? a : b;
    return m > 0 ? m : 0;
}

void px_chamfer_fill(PxCanvas *c, PxRect r, int k, uint32_t col) {
    for (int i = 0; i < r.h; i++) {
        int s = cham_inset(i, r.h, k);
        px_hline(c, r.x + s, r.y + i, r.w - 2 * s, col);
    }
}

void px_chamfer_outline(PxCanvas *c, PxRect r, int k, uint32_t col) {
    for (int i = 0; i < r.h; i++) {
        int s = cham_inset(i, r.h, k);
        if (i == 0 || i == r.h - 1) px_hline(c, r.x + s, r.y + i, r.w - 2 * s, col);
        else { px_pset(c, r.x + s, r.y + i, col); px_pset(c, r.x + r.w - 1 - s, r.y + i, col); }
    }
}

void px_bevel(PxCanvas *c, PxRect r, int k, uint32_t light) {
    PxRect in = px_inset(r, 1);
    int kk = k > 0 ? k - 1 : 0;
    if (in.w <= 0 || in.h <= 0) return;
    int s0 = cham_inset(0, in.h, kk);
    px_hline(c, in.x + s0, in.y, in.w - 2 * s0, light);
    for (int i = 1; i < in.h - kk - 1; i++) px_pset(c, in.x + cham_inset(i, in.h, kk), in.y + i, light);
}

void px_bevel_dark(PxCanvas *c, PxRect r, int k, uint32_t dark) {
    PxRect in = px_inset(r, 1);
    int kk = k > 0 ? k - 1 : 0;
    if (in.w <= 0 || in.h <= 0) return;
    int sl = cham_inset(in.h - 1, in.h, kk);
    px_hline(c, in.x + sl, in.y + in.h - 1, in.w - 2 * sl, dark);
    for (int i = kk + 1; i < in.h - 1; i++) px_pset(c, in.x + in.w - 1 - cham_inset(i, in.h, kk), in.y + i, dark);
}

static const uint8_t BAYER4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

void px_dither(PxCanvas *c, PxRect r, uint32_t col, int pattern, int phase) {
    int lim = pattern == PX_DITHER_25 ? 4 : pattern == PX_DITHER_75 ? 12 : pattern == PX_DITHER_12 ? 2 : 8;
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            if (BAYER4[y & 3][(x + phase) & 3] < lim) px_pset(c, x, y, col);
}

uint32_t px_hash(uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ seed * 0xcb1ab31fu;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}

void px_noise(PxCanvas *c, PxRect r, uint32_t seed, int permil, uint32_t col) {
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            if ((int)(px_hash((uint32_t)x, (uint32_t)y, seed) % 1000u) < permil) px_pset(c, x, y, col);
}

void px_cracks(PxCanvas *c, PxRect r, uint32_t seed, int n, uint32_t col) {
    if (r.w <= 2 || r.h <= 2) return;
    for (int i = 0; i < n; i++) {
        uint32_t h = px_hash((uint32_t)i, 77u, seed);
        int x = r.x + (int)(h % (uint32_t)r.w), y = r.y + (int)((h >> 12) % (uint32_t)r.h);
        int len = 4 + (int)((h >> 24) % 7u), dir = (int)(h >> 4) & 3;
        for (int s = 0; s < len; s++) {
            if (px_in(r, x, y)) px_pset(c, x, y, col);
            uint32_t q = px_hash((uint32_t)s, (uint32_t)i, seed + 1);
            int turn = (int)(q % 5u);
            int d = turn == 0 ? (dir + 1) & 3 : turn == 1 ? (dir + 3) & 3 : dir;
            static const int DX[4] = {1, 1, -1, -1}, DY[4] = {1, -1, 1, -1};
            x += DX[d]; y += (q & 8) ? DY[d] : 0;
        }
    }
}

uint32_t px_darker(uint32_t col) {
    switch (col) {
    case PX_SHADOW: case PX_DARK: case PX_MOSS: case PX_MAROON: return col == PX_MAROON ? PX_SHADOW : (col == PX_DARK ? PX_SHADOW : PX_BLACK);
    case PX_GRAY: case PX_STEEL: case PX_TEAL: return PX_DARK;
    case PX_RUST: return PX_MAROON;
    case PX_ORANGE: case PX_BRASS: return PX_RUST;
    case PX_GREEN: case PX_GOLD: return PX_MOSS;
    case PX_PARCH: case PX_FROST: case PX_WHITE: case PX_TEXT: return PX_GRAY;
    default: return col;
    }
}

void px_panel(PxCanvas *c, PxRect r, int style, uint32_t seed) {
    PxRect in = px_inset(r, 2);
    switch (style) {
    case PX_PANEL_GRAY:
        px_fill(c, r, PX_GRAY);
        px_noise(c, in, seed, 20, PX_DARK);
        px_noise(c, in, seed + 7, 5, PX_SHADOW);
        px_cracks(c, in, seed, 1 + (int)(seed % 3u), PX_DARK);
        px_noise(c, in, seed + 13, 3, PX_RUST);
        px_outline(c, r, PX_BLACK);
        px_bevel(c, r, 0, PX_PARCH);
        break;
    case PX_PANEL_DARK:
        px_fill(c, r, PX_DARK);
        px_noise(c, in, seed, 15, PX_SHADOW);
        px_noise(c, in, seed + 3, 3, PX_RUST);
        px_outline(c, r, PX_BLACK);
        px_bevel(c, r, 0, PX_GRAY);
        break;
    case PX_PANEL_MAROON:
        px_fill(c, r, PX_MAROON);
        px_noise(c, in, seed, 4, PX_SHADOW);
        for (int y = r.y; y < r.y + r.h; y++)
            for (int x = r.x; x < r.x + r.w; x++) {
                int dx = x - r.x, ex = r.x + r.w - 1 - x, dy = y - r.y, ey = r.y + r.h - 1 - y;
                int mx = dx < ex ? dx : ex, my = dy < ey ? dy : ey;
                int d = mx < my ? mx : my, dc = mx + my - 3;
                if (dc < d) d = dc < 0 ? 0 : dc;
                static const int LIM[4] = {16, 10, 4, 1};   // 100 %, 60 %, 25 %, 6 % de negro
                if (d < 4 && BAYER4[y & 3][x & 3] < LIM[d]) px_pset(c, x, y, PX_BLACK);
            }
        break;
    case PX_PANEL_BLACK:
        px_fill(c, r, PX_BLACK);
        px_outline(c, r, PX_GRAY);
        break;
    case PX_PANEL_PARCH:
        px_fill(c, r, PX_PARCH);
        px_noise(c, in, seed, 8, PX_BRASS);
        px_outline(c, r, PX_BLACK);
        break;
    case PX_PANEL_SLOT:   // ranura hundida: sombra arriba-izquierda, luz abajo-derecha
        px_fill(c, r, PX_SHADOW);
        px_outline(c, r, PX_BLACK);
        px_bevel(c, r, 0, PX_BLACK);
        px_bevel_dark(c, r, 0, PX_DARK);
        break;
    }
}

void px_diamond(PxCanvas *c, int cx, int cy, int rad, uint32_t col) {
    for (int dy = -rad; dy <= rad; dy++) {
        int w = rad - abs(dy);
        px_hline(c, cx - w, cy + dy, 2 * w + 1, col);
    }
}

void px_separator(PxCanvas *c, int x, int y, int w, uint32_t col) {
    int cx = x + w / 2;
    px_hline(c, x, y, cx - 4 - x, col);
    px_hline(c, cx + 5, y, x + w - (cx + 5), col);
    px_diamond(c, cx, y, 2, col);
}

void px_focus_ants(PxCanvas *c, PxRect r, int k, int phase) {
    PxRect o = pxr(r.x - 1, r.y - 1, r.w + 2, r.h + 2);
    for (int i = 0; i < o.h; i++) {
        int s = cham_inset(i, o.h, k), y = o.y + i;
        if (i == 0 || i == o.h - 1) {
            for (int x = o.x + s; x < o.x + o.w - s; x++) px_pset(c, x, y, ((x + y + phase) & 1) ? PX_FROST : PX_WHITE);
        } else {
            int xl = o.x + s, xr = o.x + o.w - 1 - s;
            px_pset(c, xl, y, ((xl + y + phase) & 1) ? PX_FROST : PX_WHITE);
            px_pset(c, xr, y, ((xr + y + phase) & 1) ? PX_FROST : PX_WHITE);
        }
    }
}

// Recorre el perimetro del octogono en sentido horario desde arriba al centro.
// Si c != NULL pinta los primeros 'limit' pixeles. Devuelve la cantidad total.
static int ring_walk(PxCanvas *c, PxRect r, int k, int limit, uint32_t col) {
    int x1 = r.x + r.w - 1, y1 = r.y + r.h - 1, cx = r.x + r.w / 2;
    int P[10][2] = {{cx, r.y}, {x1 - k, r.y}, {x1, r.y + k}, {x1, y1 - k}, {x1 - k, y1}, {r.x + k, y1},
                    {r.x, y1 - k}, {r.x, r.y + k}, {r.x + k, r.y}, {cx - 1, r.y}};
    int n = 0;
    if (c && limit > 0) px_pset(c, P[0][0], P[0][1], col);
    n++;
    for (int s = 0; s < 9; s++) {
        int x = P[s][0], y = P[s][1], tx = P[s + 1][0], ty = P[s + 1][1];
        while (x != tx || y != ty) {
            x += (tx > x) - (tx < x);
            y += (ty > y) - (ty < y);
            if (c && n < limit) px_pset(c, x, y, col);
            n++;
        }
    }
    return n;
}

void px_ring_progress(PxCanvas *c, PxRect r, int k, double t, uint32_t col) {
    if (t <= 0) return;
    if (t > 1) t = 1;
    PxRect o = pxr(r.x - 1, r.y - 1, r.w + 2, r.h + 2);
    int n1 = ring_walk(NULL, r, k, 0, 0), n2 = ring_walk(NULL, o, k, 0, 0);
    ring_walk(c, r, k, (int)(t * n1 + 0.5), col);
    ring_walk(c, o, k, (int)(t * n2 + 0.5), col);
}

// ---- iconos
uint32_t px_char_color(char ch) {
    switch (ch) {
    case 'k': return PX_BLACK;  case 'd': return PX_SHADOW; case 'g': return PX_GRAY;
    case 'p': return PX_PARCH;  case 'w': return PX_WHITE;  case 't': return PX_TEXT;
    case 'f': return PX_FROST;  case 'b': return PX_BRASS;  case 'y': return PX_GOLD;
    case 'r': return PX_MAROON; case 'o': return PX_ORANGE; case 's': return PX_STEEL;
    case 'u': return PX_RUST;   case 'v': return PX_GREEN;  case 'c': return PX_TEAL;
    case 'm': return PX_MOSS;   case 'D': return PX_DARK;
    default: return PX_NONE;
    }
}

static char icon_at(const PxIcon *ic, int x, int y) {
    if (x < 0 || y < 0 || x >= ic->w || y >= ic->h) return '.';
    return ic->rows[y * (ic->w + 1) + x];
}

void px_icon(PxCanvas *c, int x, int y, const PxIcon *ic, int flags, uint32_t mono_col) {
    if (flags & PX_ICON_OUTLINE)
        for (int j = -1; j <= ic->h; j++)
            for (int i = -1; i <= ic->w; i++)
                if (icon_at(ic, i, j) == '.' &&
                    (icon_at(ic, i - 1, j) != '.' || icon_at(ic, i + 1, j) != '.' ||
                     icon_at(ic, i, j - 1) != '.' || icon_at(ic, i, j + 1) != '.'))
                    px_pset(c, x + i, y + j, PX_BLACK);
    for (int j = 0; j < ic->h; j++)
        for (int i = 0; i < ic->w; i++) {
            char ch = icon_at(ic, i, j);
            if (ch == '.') continue;
            px_pset(c, x + i, y + j, (flags & PX_ICON_MONO) ? mono_col : px_char_color(ch));
        }
}

const PxIcon PX_ICON_DIE = {7, 7,
    "wwwwwwf wkwwwkf wwwwwwf wwwkwwf wwwwwwf wkwwwkf fffffff"};
const PxIcon PX_ICON_SKULL = {7, 7,
    ".wwwww. wwwwwww wkkwkkw wkkwkkw wwwkwww .wwwww. .wkwkw."};
const PxIcon PX_ICON_RUN = {7, 9,
    ".....ww .....ww ..wwww. .w.www. w..ww.w ...www. ..w..w. .w...w. w....ww"};
const PxIcon PX_ICON_MOUNTAIN = {11, 7,
    "...w....... ..wpw...... .wpggw..p.. .pgggg.ppg. pgggggpgggg ggggggggggg ggggggggggg"};
const PxIcon PX_ICON_FIRE = {7, 8,
    "...y... ..yy... ..yoy.. .yooyy. .yooroy .yorroy uuuuuuu .u...u."};
const PxIcon PX_ICON_CLOSE = {5, 5, "w...w .w.w. ..w.. .w.w. w...w"};
const PxIcon PX_ICON_MIN = {5, 5, "..... ..... ..... ..... wwwww"};

// ---- escalado entero + post-proceso
void px_upscale(const PxCanvas *c, uint32_t *dst, int s, int post) {
    int W = c->w * s;
    for (int y = 0; y < c->h; y++) {
        uint32_t *row = dst + (size_t)y * s * W;
        const uint32_t *src = c->px + (size_t)y * c->w;
        for (int x = 0; x < c->w; x++) for (int k = 0; k < s; k++) row[x * s + k] = src[x];
        for (int k = 1; k < s; k++) memcpy(row + (size_t)k * W, row, (size_t)W * 4);
        if ((post & PX_POST_SCANLINES) && s >= 2) {   // oscurece la ultima fila fisica de cada fila logica
            uint32_t *sl = row + (size_t)(s - 1) * W;
            int f = s >= 3 ? 179 : 210;               // ~70 % / ~82 %
            for (int x = 0; x < W; x++) {
                uint32_t p = sl[x];
                sl[x] = 0xFF000000u | ((((p >> 16) & 255) * f >> 8) << 16) | ((((p >> 8) & 255) * f >> 8) << 8) | ((p & 255) * f >> 8);
            }
        }
    }
    if (post & PX_POST_VIGNETTE) {
        int H = c->h * s;
        for (int y = 0; y < H; y++) {
            float ny = (2.0f * y - H) / H;
            for (int x = 0; x < W; x++) {
                float nx = (2.0f * x - W) / W, d = nx * nx + ny * ny - 0.6f;
                if (d <= 0) continue;
                int f = (int)(256 * (1.0f - 0.35f * (d > 1.4f ? 1.0f : d / 1.4f)));
                uint32_t p = dst[(size_t)y * W + x];
                dst[(size_t)y * W + x] = 0xFF000000u | ((((p >> 16) & 255) * f >> 8) << 16) | ((((p >> 8) & 255) * f >> 8) << 8) | ((p & 255) * f >> 8);
            }
        }
    }
}
