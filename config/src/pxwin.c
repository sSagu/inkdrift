// pxwin: ventana, escalado entero por DPI, presentacion y captura PNG (ver pxwin.h).
#include "pxwin.h"
#include <stdlib.h>

#define PXWIN_CLASS L"PxToolkitWin"
#define TIMER_ID 1

int pxwin_pick_scale(int dpi, int ww, int wh, int cw, int ch) {
    int s = 2 * dpi / 96;
    if (s < 2) s = 2;
    while (s > 1 && (cw * s > ww * 85 / 100 || ch * s > wh * 85 / 100)) s--;
    return s;
}

static void ensure_dib(PxWin *w) {
    int dw = w->canvas.w * w->scale, dh = w->canvas.h * w->scale;
    if (w->dib && w->dw == dw && w->dh == dh) return;
    if (w->dib) DeleteObject(w->dib);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = dw;
    bi.bmiHeader.biHeight = -dh;   // de arriba hacia abajo
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void *bits = NULL;
    w->dib = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    w->bits = (uint32_t *)bits;
    w->dw = dw; w->dh = dh;
}

static void present(PxWin *w, HDC dc) {
    ensure_dib(w);
    if (!w->bits) return;
    GdiFlush();
    px_upscale(&w->canvas, w->bits, w->scale, w->post);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, w->dib);
    BitBlt(dc, 0, 0, w->dw, w->dh, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
}

static void run_frame(PxWin *w) {
    ui_begin(&w->ui, GetTickCount());
    if (w->frame) w->frame(w, w->user);
    ui_end(&w->ui);
    if (w->hwnd) SetTimer(w->hwnd, TIMER_ID, w->ui.anim ? 33 : 250, NULL);
}

void pxwin_redraw(PxWin *w) {
    if (!w->hwnd || IsIconic(w->hwnd)) return;
    run_frame(w);
    HDC dc = GetDC(w->hwnd);
    present(w, dc);
    ReleaseDC(w->hwnd, dc);
}

static void place(PxWin *w, const RECT *near_rc) {
    HMONITOR mon = MonitorFromRect(near_rc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof mi};
    GetMonitorInfoW(mon, &mi);
    int ww = mi.rcWork.right - mi.rcWork.left, wh = mi.rcWork.bottom - mi.rcWork.top;
    w->scale = pxwin_pick_scale((int)GetDpiForWindow(w->hwnd), ww, wh, w->canvas.w, w->canvas.h);
    int W = w->canvas.w * w->scale, H = w->canvas.h * w->scale;
    SetWindowPos(w->hwnd, NULL, mi.rcWork.left + (ww - W) / 2, mi.rcWork.top + (wh - H) / 2, W, H, SWP_NOZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    PxWin *w = (PxWin *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        w = (PxWin *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)w);
        w->hwnd = h;
    }
    if (!w) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        run_frame(w);
        present(w, dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER: pxwin_redraw(w); return 0;
    case WM_NCHITTEST: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        ScreenToClient(h, &p);
        if (p.x >= 0 && p.y >= 0 && px_in(w->ui.caption, p.x / w->scale, p.y / w->scale)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_DPICHANGED: {   // recalcula la escala entera; ignora el tamano sugerido
        place(w, (RECT *)lp);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_CLOSE:
        if (!w->on_close || w->on_close(w, w->user)) DestroyWindow(h);
        else pxwin_redraw(w);
        return 0;
    case WM_DESTROY:
        w->hwnd = NULL;
        PostQuitMessage(0);
        return 0;
    }
    if (ui_win32_event(&w->ui, h, m, wp, lp, w->scale)) {
        pxwin_redraw(w);
        if (m == WM_SYSKEYDOWN || m == WM_SYSKEYUP) return DefWindowProcW(h, m, wp, lp);   // Alt+F4
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int pxwin_create(PxWin *w, const char *title, int cw, int ch, PxFrameFn frame, PxCloseFn on_close, void *user) {
    HINSTANCE inst = GetModuleHandleW(NULL);
    WNDCLASSW wc = {0};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = PXWIN_CLASS;
    RegisterClassW(&wc);
    if (!px_canvas_init(&w->canvas, cw, ch)) return 0;
    ui_init(&w->ui, &w->canvas);
    w->frame = frame; w->on_close = on_close; w->user = user;
    w->scale = 2;
    wchar_t wt[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wt, 256);
    POINT cur; GetCursorPos(&cur);
    CreateWindowExW(WS_EX_APPWINDOW, PXWIN_CLASS, wt, WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
                    cur.x, cur.y, cw * 2, ch * 2, NULL, NULL, inst, w);
    if (!w->hwnd) return 0;
    RECT rc = {cur.x, cur.y, cur.x + 1, cur.y + 1};
    place(w, &rc);
    ShowWindow(w->hwnd, SW_SHOW);
    UpdateWindow(w->hwnd);
    return 1;
}

void pxwin_close(PxWin *w) { if (w->hwnd) DestroyWindow(w->hwnd); }

int pxwin_run(PxWin *w) {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (w->dib) DeleteObject(w->dib);
    px_canvas_free(&w->canvas);
    return (int)msg.wParam;
}

// ---- GDI+ (API plana declarada a mano: los headers de MinGW son C++)
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GdipStartupIn;
typedef struct GpImage GpImage;
int WINAPI GdiplusStartup(ULONG_PTR *token, const GdipStartupIn *in, void *out);
void WINAPI GdiplusShutdown(ULONG_PTR token);
int WINAPI GdipCreateBitmapFromScan0(INT w, INT h, INT stride, INT fmt, BYTE *scan0, GpImage **bmp);
int WINAPI GdipSaveImageToFile(GpImage *img, const WCHAR *path, const CLSID *clsid, const void *params);
int WINAPI GdipDisposeImage(GpImage *img);

int px_save_png(const wchar_t *path, const uint32_t *bgra, int w, int h) {
    static const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    GdipStartupIn in = {1, NULL, FALSE, FALSE};
    ULONG_PTR tok;
    if (GdiplusStartup(&tok, &in, NULL) != 0) return 0;
    GpImage *bmp = NULL;
    int ok = 0;
    if (GdipCreateBitmapFromScan0(w, h, w * 4, 0x00022009 /* 32bppRGB */, (BYTE *)bgra, &bmp) == 0) {
        ok = GdipSaveImageToFile(bmp, path, &png, NULL) == 0;
        GdipDisposeImage(bmp);
    }
    GdiplusShutdown(tok);
    return ok;
}

int pxwin_shot(PxCanvas *c, int scale, int post, const wchar_t *path) {
    uint32_t *buf = (uint32_t *)malloc((size_t)c->w * scale * c->h * scale * 4);
    if (!buf) return 0;
    px_upscale(c, buf, scale, post);
    int ok = px_save_png(path, buf, c->w * scale, c->h * scale);
    free(buf);
    return ok;
}
