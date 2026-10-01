// Fondo de pantalla animado: ventanas detras de los iconos + scroll infinito.
//
// Arquitectura (pensada para gastar casi nada de CPU):
//   - El fondo es un anillo de N "franjas": ventanas hijas del escritorio, cada una de SWW
//     pixeles de ancho. Cada franja se pinta UNA sola vez (arte + papel ya mezclados).
//   - Para hacer scroll no se repinta nada: cada cuadro solo se MUEVEN las ventanas
//     (DeferWindowPos) y Windows (DWM) las compone con la GPU.
//   - Una franja que sale por la izquierda se reutiliza a la derecha con el contenido nuevo.
//   - Un hilo de trabajo genera el paisaje (world_load) y dibuja las franjas por adelantado.
#include <windows.h>
#include <shellapi.h>
#include "core.h"
#include "gen.h"
#include "plan.h"
#include "paper.h"
#include "gfx.h"
#include "view.h"
#include "config.h"
#include "wp.h"
#include <stdarg.h>

extern int g_skip_invisible;

enum { SWW = 576, NPEND = 2, WM_TRAY = WM_USER + 1, ID_EXIT = 1, ID_PAUSE = 2, ID_RESEED = 3,
       WM_RELOAD = WM_APP + 10 };                 // 0x800A: recargar config.ini (docs\config-spec.md)
enum { SEED_KEEP = 0, SEED_RANDOM = 1, SEED_SET = 2 };   // que hacer con la semilla al regenerar
static const double ZOOM = 1.142;                // calcViewBox() del original
static const double WIN_W = 3000, WIN_H = 800;   // windx/windy del original

typedef struct { HWND hwnd; volatile long long idx; } Slot;
typedef struct { uint8_t *buf; volatile LONG state; volatile long long idx; } Pending;   // 0 libre, 2 lista

// Argumentos de linea de comandos: se guardan para volver a aplicarlos (encima del ini) en
// cada recarga, asi la prioridad es siempre: por defecto < config.ini < linea de comandos.
typedef struct {
    int has_seed, has_speed, has_fps, has_zoom, nopause, allow_multi;
    char seed[CFG_SEED_BUF];
    double speed, zoom;
    int fps;
} Cli;

static struct {
    HWND ctl;                     // ventana de control (tray + temporizador)
    int W, H, N;
    volatile double scale;        // pixeles por unidad del mundo (lo escribe solo el trabajador)
    double base_scale;            // escala con zoom 1
    volatile double speed;        // unidades por segundo
    int fps;
    char seed[CFG_SEED_BUF];      // semilla efectiva (la escribe solo el trabajador)
    volatile double P;            // posicion de scroll en pixeles
    volatile LONG paused, quit, reseed, needclear, resetP;
    UINT taskbar_msg;             // "TaskbarCreated": Explorer se reinicio
    volatile int nopause;
    Slot *slots;
    Pending pend[NPEND];
    PaperTile paper;
    HANDLE worker;
    NOTIFYICONDATAA nid;
    HBRUSH bg;
    LARGE_INTEGER freq, last, start;
    long long last_P;
    char shot[520];               // --shot <png>: guarda lo que se ve y sale (UTF-8)
    double shot_after;
    // configuracion
    Cli cli;
    wchar_t cfg_path[1024];       // config.ini en uso
    char cfg_seed[CFG_SEED_BUF];  // semilla pedida por la configuracion ("" = al azar)
    double zoom;                  // zoom en uso
    HANDLE mutex;                 // instancia unica
    // pedido de regeneracion (UI -> trabajador), protegido por lock
    CRITICAL_SECTION lock;
    int next_mode;
    char next_seed[CFG_SEED_BUF];
    double next_scale;
    unsigned next_el, cur_el;          // mascara de elementos pedida / en uso
} G;

static void logmsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

// ------------------------------------------------------------ hilo de trabajo ----
static void make_seed(void) {
    if (!G.seed[0]) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER u = {{ft.dwLowDateTime, ft.dwHighDateTime}};
        unsigned long long ms = (u.QuadPart - 116444736000000000ull) / 10000ull;   // como Date.now()
        snprintf(G.seed, sizeof G.seed, "%llu", ms);
    }
}

static void worker_init_world(void) {
    prng_seed_str(G.seed);
    world_load(0, WIN_W);                               // update() inicial de la web
    static uint8_t paper512[512 * 512 * 3];
    paper_make(paper512);                               // consume aleatorios como el bgcanv
    if (G.paper.rgb) free(G.paper.rgb);
    G.paper = paper_scale(paper512, G.scale / ZOOM);
}

// Dibuja la franja k (pixeles de mundo [k*SWW, (k+1)*SWW)) con el papel ya mezclado.
static void render_strip(uint8_t *buf, long long k) {
    double x0 = (double)(k * SWW) / G.scale;
    // Cercania real: el borde de abajo de la vista queda fijo en el horizonte del original
    // (y = 800/1.142). Alejarse muestra mas mundo y mas cielo; acercarse, mas primer plano.
    double y0 = WIN_H / ZOOM - G.H / G.scale;
    gfx_render(buf, SWW, G.H, x0, y0, G.scale);
    const PaperTile *p = &G.paper;
    long long wx = k * SWW;
    int px0 = (int)(((wx % p->size) + p->size) % p->size);
    for (int y = 0; y < G.H; y++) {
        const uint8_t *prow = p->rgb + (size_t)(y % p->size) * p->size * 3;
        uint8_t *d = buf + (size_t)y * SWW * 4;
        int px = px0;
        for (int x = 0; x < SWW; x++) {
            const uint8_t *pp = prow + (size_t)px * 3;
            unsigned t0 = d[0] * pp[2] + 128, t1 = d[1] * pp[1] + 128, t2 = d[2] * pp[0] + 128;
            d[0] = (uint8_t)((t0 + (t0 >> 8)) >> 8);
            d[1] = (uint8_t)((t1 + (t1 >> 8)) >> 8);
            d[2] = (uint8_t)((t2 + (t2 >> 8)) >> 8);
            d[3] = 255;
            d += 4;
            if (++px == p->size) px = 0;
        }
    }
}

static DWORD WINAPI worker_main(LPVOID arg) {
    (void)arg;
    for (;;) {
        if (G.quit) return 0;
        // tomar el pedido pendiente (semilla / escala). Se hace bajo el mismo candado con el que
        // la UI lo escribe y levanta G.reseed, asi ningun pedido se pierde: si llega otro
        // despues de esto, el bucle de abajo lo ve en G.reseed.
        EnterCriticalSection(&G.lock);
        G.reseed = 0;
        if (G.next_mode == SEED_RANDOM) G.seed[0] = 0;
        else if (G.next_mode == SEED_SET) memcpy(G.seed, G.next_seed, sizeof G.seed);
        G.next_mode = SEED_KEEP;
        G.scale = G.next_scale;
        g_el_off = G.next_el;
        LeaveCriticalSection(&G.lock);
        make_seed();
        for (int i = 0; i < G.N; i++) G.slots[i].idx = LLONG_MIN;
        for (int i = 0; i < NPEND; i++) G.pend[i].state = 0;
        g_skip_invisible = 1;
        worker_init_world();

        long long next = 0;
        while (!G.quit && !G.reseed) {
            long long kmin = (long long)floor(G.P / SWW);
            if (next < kmin) next = kmin;
            if (next <= kmin + G.N - 1) {                    // hay una franja libre (ya salio de pantalla)
                Pending *pd = NULL;
                for (int i = 0; i < NPEND; i++) if (G.pend[i].state == 0) { pd = &G.pend[i]; break; }
                if (pd) {
                    double cursx = G.P / G.scale;
                    world_load(cursx, cursx + WIN_W);        // chunkloader(cursx, cursx + windx)
                    render_strip(pd->buf, next);
                    pd->idx = next;
                    MemoryBarrier();
                    pd->state = 2;
                    next++;
                    if ((next & 3) == 0) world_prune(cursx - 1800);
                    continue;
                }
            }
            Sleep(30);
        }
        if (G.quit) return 0;
        // regenerar (nueva semilla o zoom): reiniciar el mundo. El hilo de la interfaz es el unico
        // que escribe G.P: el trabajador solo levanta la bandera y espera a que P vuelva a 0.
        // La semilla y la escala nuevas se toman al principio del bucle.
        world_reset();
        G.resetP = 1;
        while (G.resetP && !G.quit) Sleep(10);
    }
}

// ------------------------------------------------------- ventanas / escritorio ----
static HWND find_layer(HWND *below) {
    HWND progman = FindWindowA("Progman", NULL);
    SendMessageTimeoutA(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, NULL);
    HWND w = FindWindowExA(progman, NULL, "WorkerW", NULL);
    if (w) { *below = FindWindowExA(progman, NULL, "SHELLDLL_DefView", NULL); return progman; }
    HWND v = NULL;
    while ((v = FindWindowExA(NULL, v, "WorkerW", NULL)) != NULL)
        if (FindWindowExA(v, NULL, "SHELLDLL_DefView", NULL)) { *below = HWND_TOP; return FindWindowExA(NULL, v, "WorkerW", NULL); }
    *below = HWND_BOTTOM;
    return progman;
}

// El escritorio esta listo cuando existen Progman y la vista de iconos (SHELLDLL_DefView),
// colgada de Progman o de un WorkerW.
static int desktop_ready(void) {
    HWND progman = FindWindowA("Progman", NULL);
    if (!progman) return 0;
    if (FindWindowExA(progman, NULL, "SHELLDLL_DefView", NULL)) return 1;
    HWND v = NULL;
    while ((v = FindWindowExA(NULL, v, "WorkerW", NULL)) != NULL)
        if (FindWindowExA(v, NULL, "SHELLDLL_DefView", NULL)) return 1;
    return 0;
}

// Al iniciar sesion el programa puede arrancar antes que Explorer: esperar hasta 60 s.
static int wait_for_desktop(void) {
    for (int i = 0; i < 120; i++) {
        if (desktop_ready()) {
            if (i) logmsg("escritorio listo despues de %.1f s\n", i * 0.5);
            return 1;
        }
        Sleep(500);
    }
    return desktop_ready();
}

static int covered_by_foreground(void) {
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;
    char cls[64];
    GetClassNameA(fg, cls, sizeof cls);
    if (!strcmp(cls, "Progman") || !strcmp(cls, "WorkerW") || !strcmp(cls, "Shell_TrayWnd")) return 0;
    if (IsIconic(fg) || !IsWindowVisible(fg)) return 0;
    RECT r;
    GetWindowRect(fg, &r);
    long long cw = (long long)(fmin(r.right, G.W) - fmax(r.left, 0));
    long long ch = (long long)(fmin(r.bottom, G.H) - fmax(r.top, 0));
    return cw > 0 && ch > 0 && cw * ch * 100 >= (long long)G.W * G.H * 85;
}

static LRESULT CALLBACK strip_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) { PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); return 0; }   // el contenido persiste
    return DefWindowProcA(h, msg, wp, lp);
}

static void blit_strip(HWND h, const uint8_t *buf) {
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = SWW;
    bi.bmiHeader.biHeight = -G.H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(h);
    StretchDIBits(dc, 0, 0, SWW, G.H, 0, 0, SWW, G.H, buf, &bi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(h, dc);
}

static void clear_strips(void) {
    RECT r = {0, 0, SWW, G.H};
    for (int i = 0; i < G.N; i++) {
        HDC dc = GetDC(G.slots[i].hwnd);
        FillRect(dc, &r, G.bg);
        ReleaseDC(G.slots[i].hwnd, dc);
    }
}

static long long floordiv(long long a, long long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// Mueve las franjas a su lugar para el scroll Pint (sin repintar nada).
static void place_strips(long long Pint) {
    long long kmin = floordiv(Pint, SWW);
    HDWP dw = BeginDeferWindowPos(G.N);
    for (int s = 0; s < G.N; s++) {
        long long off = (((s - kmin) % G.N) + G.N) % G.N;
        long long k = kmin + off;
        int x = (int)(k * SWW - Pint);
        dw = DeferWindowPos(dw, G.slots[s].hwnd, NULL, x, 0, 0, 0,
                            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    }
    EndDeferWindowPos(dw);
}

static void save_shot(void) {
    HDC sdc = GetDC(NULL);
    HDC mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = G.W;
    bi.bmiHeader.biHeight = -G.H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = SelectObject(mdc, bmp);
    long long Pint = (long long)G.P, kmin = floordiv(Pint, SWW);
    for (int s = 0; s < G.N; s++) {
        long long k = kmin + ((((s - kmin) % G.N) + G.N) % G.N);
        int x = (int)(k * SWW - Pint);
        HDC wdc = GetDC(G.slots[s].hwnd);
        BitBlt(mdc, x, 0, SWW, G.H, wdc, 0, 0, SRCCOPY);
        ReleaseDC(G.slots[s].hwnd, wdc);
    }
    wchar_t wp[600];
    if (!MultiByteToWideChar(CP_UTF8, 0, G.shot, -1, wp, 600)) wp[0] = 0;
    int ok = gfx_save_png((uint8_t *)bits, G.W, G.H, wp);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    fprintf(stderr, "shot: ok=%d P=%lld t=%.3f\n", ok, Pint, (double)(now.QuadPart - G.start.QuadPart) / G.freq.QuadPart);
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
}

static void tray_menu(HWND h) {
    HMENU m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_PAUSE, G.paused == 2 ? "Reanudar" : "Pausar");
    AppendMenuA(m, MF_STRING, ID_RESEED, "Nuevo paisaje");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, ID_EXIT, "Salir");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(h);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(m);
}

// ------------------------------------------------------------------ configuracion ----
// por defecto < config.ini < linea de comandos, y recorte a rangos. Devuelve 1 si leyo el ini.
static int effective_config(Config *c) {
    config_defaults(c);
    int found = config_load(c, G.cfg_path);
    if (G.cli.has_seed) config_set_seed(c, G.cli.seed);
    if (G.cli.has_speed) c->speed = G.cli.speed;
    if (G.cli.has_fps) c->fps = G.cli.fps;
    if (G.cli.has_zoom) c->zoom = G.cli.zoom;
    if (G.cli.nopause) c->pause_when_covered = 0;
    config_clamp(c);
    return found;
}

static void log_config(const char *what, const Config *c, int found, int regen) {
    char path[2048];
    if (!WideCharToMultiByte(CP_UTF8, 0, G.cfg_path, -1, path, sizeof path, NULL, NULL)) path[0] = 0;
    char extra[64] = "";
    if (regen >= 0) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        snprintf(extra, sizeof extra, " regenera=%d t=%.3f", regen,
                 (double)(now.QuadPart - G.start.QuadPart) / G.freq.QuadPart);
    }
    // una sola escritura por linea (quien lee el registro no ve lineas a medias)
    logmsg("%s: archivo=\"%s\" (%s) seed=\"%s\" speed=%g fps=%d zoom=%g pause_when_covered=%d%s\n",
           what, path, found ? "leido" : "no existe, valores por defecto", c->seed, c->speed, c->fps,
           c->zoom, c->pause_when_covered, extra);
}

// Pide al trabajador que regenere el paisaje. Solo la llama el hilo de la interfaz.
static void request_regen(int seed_mode, const char *seed, double scale) {
    EnterCriticalSection(&G.lock);
    if (seed_mode != SEED_KEEP) {                // KEEP no pisa un pedido de semilla anterior
        G.next_mode = seed_mode;
        if (seed_mode == SEED_SET) memcpy(G.next_seed, seed, sizeof G.next_seed);
    }
    G.next_scale = scale;
    G.next_el = G.cur_el;
    G.reseed = 1;
    LeaveCriticalSection(&G.lock);
}

// WM_APP+10: speed/fps/pausa al instante; si cambio la semilla o el zoom, se regenera.
static void reload_config(HWND h) {
    Config c;
    int found = effective_config(&c);
    G.speed = c.speed;
    if (c.fps != G.fps) {
        G.fps = c.fps;
        SetTimer(h, 1, 1000 / G.fps, NULL);      // mismo id: reemplaza el temporizador
    }
    G.nopause = !c.pause_when_covered;
    if (G.nopause && G.paused == 1) G.paused = 0;   // la pausa manual (2) se respeta
    int seed_changed = strcmp(c.seed, G.cfg_seed) != 0;
    int zoom_changed = c.zoom != G.zoom;
    int el_changed = c.el_off != G.cur_el;
    if (el_changed) G.cur_el = c.el_off;      // request_regen lo pasa al trabajador
    if (seed_changed || zoom_changed || el_changed) {
        // si solo cambio el zoom se conserva el paisaje actual (aunque la semilla sea al azar)
        int mode = !seed_changed ? SEED_KEEP : c.seed[0] ? SEED_SET : SEED_RANDOM;
        request_regen(mode, c.seed, G.base_scale * c.zoom);
        memcpy(G.cfg_seed, c.seed, sizeof G.cfg_seed);
        G.zoom = c.zoom;
    }
    log_config("recarga", &c, found, seed_changed || zoom_changed || el_changed);
}

static void utf8_of(const wchar_t *w, char *out, int n) {
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL)) out[0] = 0;
}

static void parse_cli(void) {
    int wargc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) return;
    char tmp[2048];
    for (int i = 1; i < wargc; i++) {
        const wchar_t *a = wargv[i];
        int more = i + 1 < wargc;
        if (!wcscmp(a, L"--seed") && more) {
            utf8_of(wargv[++i], tmp, sizeof tmp);
            Config t;
            config_set_seed(&t, tmp);
            memcpy(G.cli.seed, t.seed, sizeof G.cli.seed);
            G.cli.has_seed = 1;
        }
        else if (!wcscmp(a, L"--speed") && more) { G.cli.speed = _wtof(wargv[++i]); G.cli.has_speed = 1; }
        else if (!wcscmp(a, L"--fps") && more) { G.cli.fps = _wtoi(wargv[++i]); G.cli.has_fps = 1; }
        else if (!wcscmp(a, L"--zoom") && more) { G.cli.zoom = _wtof(wargv[++i]); G.cli.has_zoom = 1; }
        else if (!wcscmp(a, L"--nopause")) G.cli.nopause = 1;
        else if (!wcscmp(a, L"--allow-multi")) G.cli.allow_multi = 1;
        else if (!wcscmp(a, L"--config") && more) {
            wcsncpy(G.cfg_path, wargv[++i], sizeof G.cfg_path / sizeof G.cfg_path[0] - 1);
            // ruta relativa -> absoluta (la recarga puede ocurrir mucho despues)
            wchar_t full[1024];
            DWORD n = GetFullPathNameW(G.cfg_path, 1024, full, NULL);
            if (n > 0 && n < 1024) wcscpy(G.cfg_path, full);
        }
        else if (!wcscmp(a, L"--shot") && more) { utf8_of(wargv[++i], G.shot, sizeof G.shot); G.cli.allow_multi = 1; }
        else if (!wcscmp(a, L"--after") && more) G.shot_after = _wtof(wargv[++i]);
    }
    LocalFree(wargv);
}

static LRESULT CALLBACK ctl_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (G.taskbar_msg && msg == G.taskbar_msg) {
        // Explorer se reinicio: el escritorio y la bandeja son nuevos y nuestras ventanas
        // hijas se perdieron. Lo mas simple y robusto es relanzar el programa.
        char cmd[1024];
        snprintf(cmd, sizeof cmd, "%s", GetCommandLineA());
        Sleep(1500);                                  // dejar que Explorer termine de arrancar
        if (G.mutex) { CloseHandle(G.mutex); G.mutex = NULL; }   // si no, el nuevo saldria enseguida
        STARTUPINFOA si = {sizeof si};
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        DestroyWindow(h);
        return 0;
    }
    switch (msg) {
    case WM_TIMER: {
        static int tick;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - G.last.QuadPart) / G.freq.QuadPart;
        G.last = now;

        if (G.shot[0] && (double)(now.QuadPart - G.start.QuadPart) / G.freq.QuadPart >= G.shot_after) {
            save_shot();
            PostMessage(h, WM_CLOSE, 0, 0);
            return 0;
        }
        if (G.resetP) {                       // el trabajador reinicio el mundo: volver al principio
            G.P = 0;
            G.last_P = -1;
            clear_strips();
            G.resetP = 0;
            return 0;
        }

        // pasar a las franjas ya dibujadas
        for (int i = 0; i < NPEND; i++) {
            if (G.pend[i].state == 2) {
                long long k = G.pend[i].idx;
                Slot *s = &G.slots[((k % G.N) + G.N) % G.N];
                blit_strip(s->hwnd, G.pend[i].buf);
                s->idx = k;
                G.pend[i].state = 0;
            }
        }
        if (!G.nopause && (++tick % 15) == 0) {                 // cada ~0.5 s
            int cov = covered_by_foreground();
            if (G.paused != 2) G.paused = cov;
        }
        if (G.paused) return 0;
        if (dt > 0.25) dt = 0.25;
        double newP = G.P + G.speed * G.scale * dt;
        // no avanzar si la franja de la derecha todavia no esta lista
        long long kneed = floordiv((long long)newP + G.W - 1, SWW);
        Slot *sn = &G.slots[((kneed % G.N) + G.N) % G.N];
        if (sn->idx == kneed) G.P = newP;
        long long Pint = (long long)G.P;
        if (Pint != G.last_P) {
            G.last_P = Pint;
            place_strips(Pint);
        }
        return 0;
    }
    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) tray_menu(h);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_EXIT) DestroyWindow(h);
        else if (LOWORD(wp) == ID_PAUSE) G.paused = G.paused == 2 ? 0 : 2;
        else if (LOWORD(wp) == ID_RESEED) request_regen(SEED_RANDOM, NULL, G.base_scale * G.zoom);
        return 0;
    case WM_RELOAD:
        reload_config(h);
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconA(NIM_DELETE, &G.nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

int wallpaper_run(int argc, char **argv) {
    (void)argc; (void)argv;                       // se lee la linea de comandos en UTF-16 (parse_cli)
    setvbuf(stderr, NULL, _IONBF, 0);             // que los mensajes salgan enseguida aunque se redirijan
    InitializeCriticalSection(&G.lock);
    config_default_path(G.cfg_path, sizeof G.cfg_path / sizeof G.cfg_path[0]);
    parse_cli();

    // instancia unica. --shot / --allow-multi (pruebas) pueden convivir con otro fondo, pero
    // igual crean el mutex para que un tercero vea que hay uno corriendo.
    G.mutex = CreateMutexW(NULL, FALSE, L"Local\\FondoShanShuiWallpaper");
    if (G.mutex && GetLastError() == ERROR_ALREADY_EXISTS && !G.cli.allow_multi) {
        logmsg("ya hay un fondo corriendo: salgo\n");
        CloseHandle(G.mutex);
        return 0;
    }

    Config cfg;
    int found = effective_config(&cfg);
    log_config("config", &cfg, found, -1);
    memcpy(G.seed, cfg.seed, sizeof G.seed);
    memcpy(G.cfg_seed, cfg.seed, sizeof G.cfg_seed);
    G.speed = cfg.speed;
    G.fps = cfg.fps;
    G.zoom = cfg.zoom;
    G.cur_el = G.next_el = g_el_off = cfg.el_off;
    G.nopause = !cfg.pause_when_covered;

    if (!wait_for_desktop()) {
        logmsg("no aparecio el escritorio (Progman/SHELLDLL_DefView) en 60 s\n");
        return 2;
    }

    G.W = GetSystemMetrics(SM_CXSCREEN);
    G.H = GetSystemMetrics(SM_CYSCREEN);
    G.base_scale = G.H / (WIN_H / ZOOM);          // la ventana de la web muestra 800/1.142 unidades de alto
    G.scale = G.next_scale = G.base_scale * G.zoom;
    G.next_mode = SEED_KEEP;
    logmsg("pantalla %dx%d escala=%.4f px/u\n", G.W, G.H, G.scale);
    QueryPerformanceFrequency(&G.freq);
    QueryPerformanceCounter(&G.last);
    G.start = G.last;
    G.last_P = -1;
    G.bg = CreateSolidBrush(RGB(240, 230, 210));

    G.N = (G.W + SWW - 1) / SWW + 3;              // visibles + 1 de borde + 2 de reserva
    G.slots = calloc((size_t)G.N, sizeof(Slot));
    for (int i = 0; i < G.N; i++) G.slots[i].idx = LLONG_MIN;
    for (int i = 0; i < NPEND; i++) G.pend[i].buf = malloc((size_t)SWW * G.H * 4);
    if (!gfx_init()) return 1;

    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA sc = {0};
    sc.lpfnWndProc = strip_proc;
    sc.hInstance = inst;
    sc.lpszClassName = "FondoShanShuiStrip";
    RegisterClassA(&sc);
    WNDCLASSA cc = {0};
    cc.lpfnWndProc = ctl_proc;
    cc.hInstance = inst;
    cc.lpszClassName = "FondoShanShuiCtl";
    RegisterClassA(&cc);

    HWND below;
    HWND layer = find_layer(&below);
    for (int i = 0; i < G.N; i++) {
        HWND h = CreateWindowExA(0, sc.lpszClassName, "Fondo", WS_POPUP, G.W + i * SWW, 0, SWW, G.H, NULL, NULL, inst, NULL);
        SetParent(h, layer);
        LONG_PTR style = GetWindowLongPtrA(h, GWL_STYLE);
        SetWindowLongPtrA(h, GWL_STYLE, (style & ~WS_POPUP) | WS_CHILD);
        // Progman no tiene superficie propia (NOREDIRECTIONBITMAP): hace falta WS_EX_LAYERED
        LONG_PTR ex = GetWindowLongPtrA(h, GWL_EXSTYLE);
        SetWindowLongPtrA(h, GWL_EXSTYLE, ex | WS_EX_LAYERED | WS_EX_NOACTIVATE);
        SetLayeredWindowAttributes(h, 0, 255, LWA_ALPHA);
        SetWindowPos(h, below, G.W + i * SWW, 0, SWW, G.H, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        G.slots[i].hwnd = h;
    }
    clear_strips();

    G.ctl = CreateWindowExA(WS_EX_TOOLWINDOW, cc.lpszClassName, "Fondo shan-shui", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);
    // Si el programa se lanzo "como administrador", Explorer (nivel normal) no podria enviarle
    // los clics de la bandeja ni el aviso de reinicio: se permiten esos mensajes explicitamente.
    G.taskbar_msg = RegisterWindowMessageA("TaskbarCreated");
    ChangeWindowMessageFilterEx(G.ctl, WM_TRAY, 1 /* MSGFLT_ALLOW */, NULL);
    ChangeWindowMessageFilterEx(G.ctl, G.taskbar_msg, 1, NULL);
    ChangeWindowMessageFilterEx(G.ctl, WM_RELOAD, 1, NULL);      // la interfaz de configuracion
    ChangeWindowMessageFilterEx(G.ctl, WM_COMMAND, 1, NULL);
    memset(&G.nid, 0, sizeof G.nid);
    G.nid.cbSize = sizeof G.nid;
    G.nid.hWnd = G.ctl;
    G.nid.uID = 1;
    G.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    G.nid.uCallbackMessage = WM_TRAY;
    G.nid.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    strcpy(G.nid.szTip, "Fondo shan-shui (clic: menu)");
    Shell_NotifyIconA(NIM_ADD, &G.nid);

    G.worker = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
    SetThreadPriority(G.worker, THREAD_PRIORITY_BELOW_NORMAL);
    SetTimer(G.ctl, 1, 1000 / G.fps, NULL);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    G.quit = 1;
    WaitForSingleObject(G.worker, 3000);
    if (G.mutex) CloseHandle(G.mutex);
    return 0;
}
