#include "gfx.h"
#include <windows.h>

// ---- GDI+ (API plana). Los headers de MinGW son solo C++, asi que se declara lo necesario.
typedef int GpStatus;
typedef float REAL;
typedef struct GpImage GpImage;
typedef struct GpGraphics GpGraphics;
typedef struct GpBrush GpBrush;
typedef struct GpPen GpPen;
typedef struct GpFontFamily GpFontFamily;
typedef struct GpFont GpFont;
typedef struct GpStringFormat GpStringFormat;
typedef struct { REAL X, Y; } GpPointF;
typedef struct { REAL X, Y, Width, Height; } GpRectF;
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GdiplusStartupInput;

GpStatus WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *in, void *out);
void WINAPI GdiplusShutdown(ULONG_PTR token);
GpStatus WINAPI GdipCreateBitmapFromScan0(INT w, INT h, INT stride, INT fmt, BYTE *scan0, GpImage **bmp);
GpStatus WINAPI GdipGetImageGraphicsContext(GpImage *img, GpGraphics **g);
GpStatus WINAPI GdipDeleteGraphics(GpGraphics *g);
GpStatus WINAPI GdipDisposeImage(GpImage *img);
GpStatus WINAPI GdipSetSmoothingMode(GpGraphics *g, INT mode);
GpStatus WINAPI GdipSetPixelOffsetMode(GpGraphics *g, INT mode);
GpStatus WINAPI GdipGraphicsClear(GpGraphics *g, UINT32 argb);
GpStatus WINAPI GdipResetWorldTransform(GpGraphics *g);
GpStatus WINAPI GdipScaleWorldTransform(GpGraphics *g, REAL sx, REAL sy, INT order);
GpStatus WINAPI GdipTranslateWorldTransform(GpGraphics *g, REAL dx, REAL dy, INT order);
GpStatus WINAPI GdipRotateWorldTransform(GpGraphics *g, REAL angle, INT order);
GpStatus WINAPI GdipCreateSolidFill(UINT32 argb, GpBrush **brush);
GpStatus WINAPI GdipSetSolidFillColor(GpBrush *brush, UINT32 argb);
GpStatus WINAPI GdipDeleteBrush(GpBrush *brush);
GpStatus WINAPI GdipFillPolygon(GpGraphics *g, GpBrush *brush, const GpPointF *pts, INT n, INT fillMode);
GpStatus WINAPI GdipCreatePen1(UINT32 argb, REAL width, INT unit, GpPen **pen);
GpStatus WINAPI GdipSetPenColor(GpPen *pen, UINT32 argb);
GpStatus WINAPI GdipSetPenWidth(GpPen *pen, REAL width);
GpStatus WINAPI GdipSetPenMiterLimit(GpPen *pen, REAL limit);
GpStatus WINAPI GdipSetPenLineJoin(GpPen *pen, INT join);
GpStatus WINAPI GdipDeletePen(GpPen *pen);
GpStatus WINAPI GdipDrawLines(GpGraphics *g, GpPen *pen, const GpPointF *pts, INT n);
GpStatus WINAPI GdipSaveImageToFile(GpImage *img, const WCHAR *path, const CLSID *clsid, const void *params);
GpStatus WINAPI GdipCreateFontFamilyFromName(const WCHAR *name, void *coll, GpFontFamily **fam);
GpStatus WINAPI GdipDeleteFontFamily(GpFontFamily *fam);
GpStatus WINAPI GdipCreateFont(const GpFontFamily *fam, REAL em, INT style, INT unit, GpFont **font);
GpStatus WINAPI GdipDeleteFont(GpFont *font);
GpStatus WINAPI GdipGetCellAscent(const GpFontFamily *fam, INT style, UINT16 *ascent);
GpStatus WINAPI GdipGetEmHeight(const GpFontFamily *fam, INT style, UINT16 *em);
GpStatus WINAPI GdipStringFormatGetGenericTypographic(GpStringFormat **fmt);
GpStatus WINAPI GdipSetStringFormatAlign(GpStringFormat *fmt, INT align);
GpStatus WINAPI GdipDrawString(GpGraphics *g, const WCHAR *s, INT len, const GpFont *font,
                               const GpRectF *rect, const GpStringFormat *fmt, const GpBrush *brush);

enum { SMOOTH_AA = 4, PIXOFF_HALF = 4, FILL_WINDING = 1, JOIN_MITER = 0, UNIT_WORLD = 0,
       ORDER_PREPEND = 0, ORDER_APPEND = 1, PF_32BPP_RGB = 0x22009 };

static ULONG_PTR g_token;
static GpFontFamily *g_fam;

int gfx_init(void) {
    GdiplusStartupInput in = {1, NULL, FALSE, FALSE};
    if (GdiplusStartup(&g_token, &in, NULL) != 0) return 0;
    GdipCreateFontFamilyFromName(L"Verdana", NULL, &g_fam);
    return 1;
}
void gfx_shutdown(void) { GdiplusShutdown(g_token); }

static UINT32 argb_of(Col c) {
    if (c.kind == 2) return 0xFFFFFFFFu;
    double a = c.kind == 3 ? 1.0 : c.a;
    int ai = (int)floor(a * 255 + 0.5);
    if (ai < 0) ai = 0;
    if (ai > 255) ai = 255;
    return ((UINT32)ai << 24) | ((UINT32)c.r << 16) | ((UINT32)c.g << 8) | c.b;
}

static GpPointF *g_pts;
static int g_pts_cap;

void gfx_render(uint8_t *bgra, int w, int h, double x0, double y0, double scale) {
    GpImage *bmp = NULL;
    GpGraphics *g = NULL;
    if (GdipCreateBitmapFromScan0(w, h, w * 4, PF_32BPP_RGB, bgra, &bmp) != 0) return;
    GdipGetImageGraphicsContext(bmp, &g);
    GdipSetSmoothingMode(g, SMOOTH_AA);
    GdipSetPixelOffsetMode(g, PIXOFF_HALF);
    GdipGraphicsClear(g, 0xFFFFFFFFu);
    GdipScaleWorldTransform(g, (REAL)scale, (REAL)scale, ORDER_PREPEND);

    // Region visible en decimas de unidad (para descartar items fuera de la tarea).
    long long rx0 = (long long)floor(x0 * 10), ry0 = (long long)floor(y0 * 10);
    long long rx1 = rx0 + (long long)ceil(w / scale * 10), ry1 = ry0 + (long long)ceil(h / scale * 10);

    GpBrush *brush = NULL;
    GdipCreateSolidFill(0xFF000000u, &brush);
    GpPen *pen = NULL;
    GdipCreatePen1(0xFF000000u, 1, UNIT_WORLD, &pen);
    GdipSetPenMiterLimit(pen, 4);
    GdipSetPenLineJoin(pen, JOIN_MITER);

    for (int ci = 0; ci < g_world.n; ci++) {
        const Chunk *ch = g_world.chunks[ci];
        for (int ii = 0; ii < ch->n; ii++) {
            const Item *it = &ch->items[ii];
            if (it->x1 < rx0 || it->x0 > rx1 || it->y1 < ry0 || it->y0 > ry1) continue;
            if (it->type == 1) {
                if (!g_fam || it->fil.a <= 0) continue;
                GpFont *font = NULL;
                if (GdipCreateFont(g_fam, it->fsize, 0, UNIT_WORLD, &font) != 0) continue;
                UINT16 asc = 0, em = 1;
                GdipGetCellAscent(g_fam, 0, &asc);
                GdipGetEmHeight(g_fam, 0, &em);
                GpStringFormat *fmt = NULL;
                GdipStringFormatGetGenericTypographic(&fmt);
                GdipSetStringFormatAlign(fmt, 1);
                GdipSetSolidFillColor(brush, argb_of(it->fil));
                double px = it->pts[0] / 10.0 - x0;
                double py = it->pts[1] / 10.0 - y0;
                GdipTranslateWorldTransform(g, (REAL)px, (REAL)py, ORDER_PREPEND);
                GdipRotateWorldTransform(g, it->angle, ORDER_PREPEND);
                int len = (int)strlen(it->text);
                WCHAR wt[64];
                for (int k = 0; k < len && k < 63; k++) wt[k] = (WCHAR)it->text[k];
                wt[len < 63 ? len : 63] = 0;
                GpRectF rc = {0, -(REAL)(it->fsize * asc / (double)em), 0, (REAL)it->fsize * 2};
                GdipDrawString(g, wt, -1, font, &rc, fmt, brush);
                GdipResetWorldTransform(g);
                GdipScaleWorldTransform(g, (REAL)scale, (REAL)scale, ORDER_PREPEND);
                GdipDeleteFont(font);
                continue;
            }
            if (it->n > g_pts_cap) {
                g_pts_cap = it->n * 2;
                g_pts = realloc(g_pts, (size_t)g_pts_cap * sizeof(GpPointF));
            }
            for (int k = 0; k < it->n; k++) {
                g_pts[k].X = (REAL)(it->pts[2 * k] / 10.0 - x0);
                g_pts[k].Y = (REAL)(it->pts[2 * k + 1] / 10.0 - y0);
            }
            if (it->fil.kind != 1 && it->fil.a > 0 && it->n >= 3) {
                GdipSetSolidFillColor(brush, argb_of(it->fil));
                GdipFillPolygon(g, brush, g_pts, it->n, FILL_WINDING);
            }
            if (it->str.kind != 1 && it->str.a > 0 && it->wid > 0 && it->n >= 2) {
                GdipSetPenColor(pen, argb_of(it->str));
                GdipSetPenWidth(pen, it->wid);
                GdipDrawLines(g, pen, g_pts, it->n);
            }
        }
    }
    GdipDeletePen(pen);
    GdipDeleteBrush(brush);
    GdipDeleteGraphics(g);
    GdipDisposeImage(bmp);
}

int gfx_save_png(const uint8_t *bgra, int w, int h, const wchar_t *path) {
    static const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    GpImage *bmp = NULL;
    if (GdipCreateBitmapFromScan0(w, h, w * 4, PF_32BPP_RGB, (BYTE *)bgra, &bmp) != 0) return 0;
    int ok = GdipSaveImageToFile(bmp, path, &png, NULL) == 0;
    GdipDisposeImage(bmp);
    return ok;
}
