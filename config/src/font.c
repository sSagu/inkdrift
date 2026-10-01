// font: fuente bitmap dibujada a mano (ver font.h). Cada glifo es una cadena de filas separadas
// por espacios; '#' = tinta. Las filas que pasan de la altura de mayuscula son descendentes.
// Las vocales acentuadas, la U con dieresis y la N con tilde se componen: letra base + marca.
#include "font.h"
#include "i18n.h"
#include <string.h>

typedef struct { uint32_t cp; const char *rows; } Glyph;

static const Glyph SMALL[] = {
    {'A', ".#. #.# ### #.# #.#"}, {'B', "##. #.# ##. #.# ##."}, {'C', ".## #.. #.. #.. .##"},
    {'D', "##. #.# #.# #.# ##."}, {'E', "### #.. ##. #.. ###"}, {'F', "### #.. ##. #.. #.."},
    {'G', ".### #... #.## #..# .###"}, {'H', "#.# #.# ### #.# #.#"}, {'I', "### .#. .#. .#. ###"},
    {'J', "..# ..# ..# #.# .#."}, {'K', "#.# #.# ##. #.# #.#"}, {'L', "#.. #.. #.. #.. ###"},
    {'M', "#...# ##.## #.#.# #...# #...#"}, {'N', "#..# ##.# #.## #..# #..#"},
    {'O', ".##. #..# #..# #..# .##."}, {'P', "##. #.# ##. #.. #.."}, {'Q', ".##. #..# #..# #.#. .#.#"},
    {'R', "##. #.# ##. #.# #.#"}, {'S', ".## #.. .#. ..# ##."}, {'T', "### .#. .#. .#. .#."},
    {'U', "#.# #.# #.# #.# ###"}, {'V', "#.# #.# #.# #.# .#."}, {'W', "#...# #...# #.#.# ##.## #...#"},
    {'X', "#.# #.# .#. #.# #.#"}, {'Y', "#.# #.# .#. .#. .#."}, {'Z', "### ..# .#. #.. ###"},
    {'0', "### #.# #.# #.# ###"}, {'1', ".#. ##. .#. .#. ###"}, {'2', "##. ..# .#. #.. ###"},
    {'3', "##. ..# .#. ..# ##."}, {'4', "#.# #.# ### ..# ..#"}, {'5', "### #.. ##. ..# ##."},
    {'6', ".## #.. ### #.# ###"}, {'7', "### ..# .#. .#. .#."}, {'8', "### #.# ### #.# ###"},
    {'9', "### #.# ### ..# ##."},
    {'.', ". . . . #"}, {',', ".. .. .. .. .# #."}, {':', ". # . # ."}, {';', ".. .# .. .# #."},
    {'!', "# # # . #"}, {'?', "##. ..# .#. ... .#."}, {0xA1, "# . # # #"}, {0xBF, ".#. ... .#. #.. .##"},
    {'-', "... ... ### ... ..."}, {'+', "... .#. ### .#. ..."}, {'/', "..# ..# .#. #.. #.."},
    {'%', "#.# ..# .#. #.. #.#"}, {'(', ".# #. #. #. .#"}, {')', "#. .# .# .# #."},
    {'\'', "# # . . ."}, {'"', "#.# #.# ... ... ..."}, {0xB7, ". . # . ."}, {'*', "... #.# .#. #.# ..."},
    {'=', "... ### ... ### ..."}, {'<', "..# .#. #.. .#. ..#"}, {'>', "#.. .#. ..# .#. #.."},
    {'[', "## #. #. #. ##"}, {']', "## .# .# .# ##"}, {'_', "... ... ... ... ###"},
    {'#', "#.#. #### .#.# #### .#.#"}, {0x2026, "..... ..... ..... ..... #.#.#"},
    {0x2192, "..#. ...# #### ...# ..#."},
};

static const Glyph LARGE[] = {
    {'A', ".####. ##..## ##..## ###### ##..## ##..## ##..##"},
    {'B', "#####. ##..## ##..## #####. ##..## ##..## #####."},
    {'C', ".####. ##..## ##.... ##.... ##.... ##..## .####."},
    {'D', "####.. ##.##. ##..## ##..## ##..## ##.##. ####.."},
    {'E', "###### ##.... ##.... #####. ##.... ##.... ######"},
    {'F', "###### ##.... ##.... #####. ##.... ##.... ##...."},
    {'G', ".####. ##..## ##.... ##.### ##..## ##..## .#####"},
    {'H', "##..## ##..## ##..## ###### ##..## ##..## ##..##"},
    {'I', "#### .##. .##. .##. .##. .##. ####"},
    {'J', "....## ....## ....## ....## ##..## ##..## .####."},
    {'K', "##..## ##.##. ####.. ###... ####.. ##.##. ##..##"},
    {'L', "##.... ##.... ##.... ##.... ##.... ##.... ######"},
    {'M', "##...## ###.### ####### ##.#.## ##...## ##...## ##...##"},
    {'N', "##..## ###.## ###### ##.### ##..## ##..## ##..##"},
    {'O', ".####. ##..## ##..## ##..## ##..## ##..## .####."},
    {'P', "#####. ##..## ##..## #####. ##.... ##.... ##...."},
    {'Q', ".####. ##..## ##..## ##..## ##.### ##.##. .##.##"},
    {'R', "#####. ##..## ##..## #####. ##.##. ##..## ##..##"},
    {'S', ".####. ##..## ##.... .####. ....## ##..## .####."},
    {'T', "###### ..##.. ..##.. ..##.. ..##.. ..##.. ..##.."},
    {'U', "##..## ##..## ##..## ##..## ##..## ##..## .####."},
    {'V', "##..## ##..## ##..## ##..## ##..## .####. ..##.."},
    {'W', "##...## ##...## ##...## ##.#.## ####### ###.### ##...##"},
    {'X', "##..## ##..## .####. ..##.. .####. ##..## ##..##"},
    {'Y', "##..## ##..## ##..## .####. ..##.. ..##.. ..##.."},
    {'Z', "###### ....## ...##. ..##.. .##... ##.... ######"},
    {'0', ".###. ##.## ##.## ##.## ##.## ##.## .###."},
    {'1', ".##. ###. .##. .##. .##. .##. ####"},
    {'2', ".###. ##.## ...## ..##. .##.. ##... #####"},
    {'3', "####. ...## ...## .###. ...## ...## ####."},
    {'4', "##.## ##.## ##.## ##### ...## ...## ...##"},
    {'5', "##### ##... ####. ...## ...## ##.## .###."},
    {'6', ".###. ##... ##... ####. ##.## ##.## .###."},
    {'7', "##### ...## ...## ..##. .##.. .##.. .##.."},
    {'8', ".###. ##.## ##.## .###. ##.## ##.## .###."},
    {'9', ".###. ##.## ##.## .#### ...## ...## .###."},
    {'.', ".. .. .. .. .. ## ##"}, {',', ".. .. .. .. .. ## ## #."},
    {':', ".. ## ## .. .. ## ##"}, {';', ".. ## ## .. .. ## ## #."},
    {'!', "## ## ## ## ## .. ##"}, {'?', ".####. ##..## ....## ...##. ..##.. ...... ..##.."},
    {0xA1, "## .. ## ## ## ## ##"}, {0xBF, "..##.. ...... ..##.. .##... ##.... ##..## .####."},
    {'-', ".... .... .... #### .... .... ...."}, {'+', "...... ..##.. ..##.. ###### ..##.. ..##.. ......"},
    {'/', "....## ....## ...##. ..##.. .##... ##.... ##...."},
    {'%', "##...# ##..## ...##. ..##.. .##... ##..## #...##"},
    {'(', ".## ##. ##. ##. ##. ##. .##"}, {')', "##. .## .## .## .## .## ##."},
    {'\'', "## ## .. .. .. .. .."}, {'"', "##.## ##.## ..... ..... ..... ..... ....."},
    {0xB7, ".. .. .. ## ## .. .."}, {'*', "...... ##..## .####. ###### .####. ##..## ......"},
    {'=', "..... ..... ##### ..... ##### ..... ....."}, {'<', "...## ..##. .##.. ##... .##.. ..##. ...##"},
    {'>', "##... .##.. ..##. ...## ..##. .##.. ##..."}, {'[', "### ##. ##. ##. ##. ##. ###"},
    {']', "### .## .## .## .## .## ###"}, {'_', "...... ...... ...... ...... ...... ...... ######"},
    {'#', ".##.##. ####### .##.##. .##.##. ####### .##.##. ......."},
    {0x2026, "........ ........ ........ ........ ........ ##.##.## ##.##.##"},
    {0x2192, "...##.. ....##. ......# ####### ......# ....##. ...##.."},
};

// Marcas (acentos): 2 filas (o 1) que van en y-3..y-2; la fila y-1 queda libre.
enum { MK_ACUTE, MK_DIAER, MK_TILDE };
static const char *MARK_S[3] = {".# #.", "#.# ...", ".#.# #.#."};
static const char *MARK_L[3] = {"..## .##.", "##..## ......", ".##..# #..##."};
typedef struct { uint32_t cp, base; int mark; } Comp;
static const Comp COMP[] = {
    {0xC1, 'A', MK_ACUTE}, {0xC9, 'E', MK_ACUTE}, {0xCD, 'I', MK_ACUTE}, {0xD3, 'O', MK_ACUTE},
    {0xDA, 'U', MK_ACUTE}, {0xDC, 'U', MK_DIAER}, {0xD1, 'N', MK_TILDE},
};

int font_cap(FontId f) { return f == FONT_LARGE ? 7 : 5; }
int font_ascent(FontId f) { (void)f; return 3; }
int font_line_h(FontId f) { return f == FONT_LARGE ? 12 : 10; }

uint32_t utf8_next(const char **ps) {
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t c = s[0];
    int n = 0;
    if (!c) return 0;
    if (c < 0x80) n = 0;
    else if ((c & 0xE0) == 0xC0) { c &= 0x1F; n = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; n = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; n = 3; }
    else { *ps += 1; return 0xFFFD; }
    for (int i = 1; i <= n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *ps += i; return 0xFFFD; }
        c = (c << 6) | (s[i] & 0x3F);
    }
    *ps += n + 1;
    return c;
}

int utf8_put(uint32_t cp, char *o) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63)); return 3; }
    o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 63));
    o[2] = (char)(0x80 | ((cp >> 6) & 63)); o[3] = (char)(0x80 | (cp & 63));
    return 4;
}

uint32_t utf8_upper(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 0x20;   // a con tilde..y con tilde (Latin-1)
    return cp;
}

int utf8_count(const char *s) { int n = 0; while (utf8_next(&s)) n++; return n; }

int utf8_offset(const char *s, int n) {
    const char *p = s;
    while (n-- > 0 && *p) utf8_next(&p);
    return (int)(p - s);
}

// Normaliza: mayuscula + sustitutos tipograficos.
static uint32_t norm(uint32_t cp) {
    cp = utf8_upper(cp);
    switch (cp) {
    case 0x2013: case 0x2014: case 0x2212: return '-';
    case 0x2018: case 0x2019: case 0xB4: return '\'';
    case 0x201C: case 0x201D: case 0xAB: case 0xBB: return '"';
    case 0xA0: return ' ';
    case 0xD7: return 'X';
    }
    return cp;
}

static const Glyph *find(FontId f, uint32_t cp, int *mark) {
    const Glyph *t = f == FONT_LARGE ? LARGE : SMALL;
    int n = f == FONT_LARGE ? (int)(sizeof LARGE / sizeof *LARGE) : (int)(sizeof SMALL / sizeof *SMALL);
    *mark = -1;
    for (size_t i = 0; i < sizeof COMP / sizeof *COMP; i++)
        if (COMP[i].cp == cp) { *mark = COMP[i].mark; cp = COMP[i].base; break; }
    for (int i = 0; i < n; i++) if (t[i].cp == cp) return &t[i];
    return NULL;
}

static int rows_w(const char *r) { const char *e = strchr(r, ' '); return e ? (int)(e - r) : (int)strlen(r); }

static int space_w(FontId f) { return f == FONT_LARGE ? 3 : 2; }

// ancho de avance de un caracter (glifo + 1 de espacio entre letras)
static int adv(FontId f, uint32_t cp) {
    int mk;
    if (cp == ' ') return space_w(f) + 1;
    const Glyph *g = find(f, cp, &mk);
    return (g ? rows_w(g->rows) : font_cap(f) - 1) + 1;
}

int font_has(FontId f, uint32_t cp) { int mk; return cp == ' ' || find(f, norm(cp), &mk) != NULL; }

static void blit_rows(PxCanvas *c, int x, int y, const char *r, uint32_t col) {
    int cx = x;
    for (; *r; r++) {
        if (*r == ' ') { y++; cx = x; continue; }
        if (*r == '#') px_pset(c, cx, y, col);
        cx++;
    }
}

static void draw_glyph(PxCanvas *c, FontId f, int x, int y, uint32_t cp, uint32_t col) {
    int mk;
    const Glyph *g = find(f, cp, &mk);
    if (!g) {   // caracter sin glifo: caja visible para detectarlo
        int w = font_cap(f) - 1, h = font_cap(f);
        px_outline(c, pxr(x, y, w, h), col);
        return;
    }
    blit_rows(c, x, y, g->rows, col);
    if (mk >= 0) {
        const char *m = (f == FONT_LARGE ? MARK_L : MARK_S)[mk];
        int gw = rows_w(g->rows), mw = rows_w(m);
        int mx = x + (gw - mw + 1) / 2 + (mk == MK_ACUTE ? (f == FONT_LARGE ? 1 : 0) : 0);
        blit_rows(c, mx, y - 3, m, col);
    }
}

static int line_width(FontId f, const char *s, const char **end) {
    int w = 0;
    const char *p = s;
    for (;;) {
        const char *q = p;
        uint32_t cp = utf8_next(&q);
        if (!cp || cp == '\n') break;
        w += adv(f, norm(cp));
        p = q;
    }
    if (end) *end = p;
    return w > 0 ? w - 1 : 0;
}

int font_text_width(FontId f, const char *s) {
    s = i18n_tr(s);
    int best = 0;
    for (;;) {
        const char *e;
        int w = line_width(f, s, &e);
        if (w > best) best = w;
        if (*e != '\n') break;
        s = e + 1;
    }
    return best;
}

int font_draw(PxCanvas *c, FontId f, int x, int y, const char *s, uint32_t col, int flags) {
    s = i18n_tr(s);
    int last = 0;
    for (;;) {
        const char *e;
        int w = line_width(f, s, &e), cx = x;
        if (flags & TX_CENTER) cx = x - w / 2;
        else if (flags & TX_RIGHT) cx = x - w;
        for (int pass = (flags & TX_SHADOW) ? 0 : 1; pass < 2; pass++) {
            const char *p = s;
            int px = cx;
            while (p < e) {
                uint32_t cp = norm(utf8_next(&p));
                if (cp != ' ') {
                    if (pass == 0) draw_glyph(c, f, px + 1, y + 1, cp, PX_BLACK);
                    else draw_glyph(c, f, px, y, cp, col);
                }
                px += adv(f, cp);
            }
        }
        last = w;
        if (*e != '\n') break;
        s = e + 1;
        y += font_line_h(f);
    }
    return last;
}

void font_draw_in(PxCanvas *c, FontId f, PxRect r, const char *s, uint32_t col, int flags) {
    int y = r.y + (r.h - font_cap(f)) / 2;
    int x = (flags & TX_CENTER) ? r.x + r.w / 2 : (flags & TX_RIGHT) ? r.x + r.w : r.x;
    font_draw(c, f, x, y, s, col, flags);
}

int font_wrap(FontId f, const char *s, int maxw, char *out, int outsz) {
    s = i18n_tr(s);
    int o = 0, lines = 1, linew = 0;
    if (outsz <= 0) return 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '\n') { if (o < outsz - 1) out[o++] = '\n'; lines++; linew = 0; s++; continue; }
        const char *e = s;
        int ww = 0;
        while (*e && *e != ' ' && *e != '\n') { const char *q = e; ww += adv(f, norm(utf8_next(&q))); e = q; }
        int need = (linew ? adv(f, ' ') : 0) + ww;
        if (linew && linew + need - 1 > maxw) { if (o < outsz - 1) out[o++] = '\n'; lines++; linew = 0; need = ww; }
        else if (linew && o < outsz - 1) out[o++] = ' ';
        for (const char *p = s; p < e && o < outsz - 1; p++) out[o++] = *p;
        linew += need;
        s = e;
    }
    out[o] = 0;
    return lines;
}
