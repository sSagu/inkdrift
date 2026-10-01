// Programa principal.
//   fondo.exe                       -> fondo de pantalla animado (detras de los iconos)
//   fondo.exe --render <seed> <salida.png> [x] [ancho] [alto]   -> imagen de prueba
#include <windows.h>
#include "core.h"
#include "gen.h"
#include "plan.h"
#include "paper.h"
#include "gfx.h"
#include "view.h"
#include "wp.h"

extern int g_skip_invisible;

static const double ZOOM = 1.142;     // calcViewBox() del original
static const double WIN_W = 3000, WIN_H = 800;

static int render_main(int argc, char **argv) {
    const char *seed = argv[2];
    const char *out = argv[3];
    double cursx = argc > 4 ? atof(argv[4]) : 0;
    int W = argc > 5 ? atoi(argv[5]) : 1600;
    int H = argc > 6 ? atoi(argv[6]) : 800;

    g_skip_invisible = 1;
    { const char *e = getenv("WP_EL_OFF"); if (e) g_el_off = (unsigned)strtoul(e, NULL, 0); }   // pruebas
    prng_seed_str(seed);
    gfx_init();
    world_load(0, WIN_W);                       // update() inicial
    static uint8_t paper[512 * 512 * 3];
    paper_make(paper);                          // bgcanv: consume aleatorios en el mismo punto
    for (double c = 200; c <= cursx; c += 200)  // xcroll(200) repetido
        world_load(c, c + WIN_W);

    double scale = H / (WIN_H / ZOOM);
    uint8_t *art = malloc((size_t)W * H * 4);
    uint8_t *img = malloc((size_t)W * H * 4);
    gfx_render(art, W, H, cursx, 0, scale);
    if (argc > 7 && !strcmp(argv[7], "art")) {
        memcpy(img, art, (size_t)W * H * 4);        // solo el arte sobre blanco (para comparar)
    } else {
        PaperTile pt = paper_scale(paper, scale / ZOOM);
        multiply_rows(img, art, W, 0, H, &pt);
    }

    wchar_t wout[512];
    MultiByteToWideChar(CP_UTF8, 0, out, -1, wout, 512);
    int ok = gfx_save_png(img, W, H, wout);
    long items = 0;
    for (int i = 0; i < g_world.n; i++) items += g_world.chunks[i]->n;
    printf("%s  %dx%d  trozos=%d items=%ld\n", ok ? "ok" : "ERROR", W, H, g_world.n, items);
    {   // conteo por tipo (pruebas)
        static const char *T[] = {"village", "bridge", "duel", "drstone"};
        for (int k = 0; k < 4; k++) { int n = 0; for (int i = 0; i < g_world.n; i++) if (!strcmp(g_world.chunks[i]->tag, T[k])) n++; printf("  %s=%d", T[k], n); }
        printf("\n");
        for (int i = 0; i < g_world.n; i++) if (!strcmp(g_world.chunks[i]->tag, "bridge")) { printf("  primer puente x=%.0f y=%.0f\n", g_world.chunks[i]->x, g_world.chunks[i]->y); break; }
    }
    return ok ? 0 : 1;
}

// --region <seed> <salida.png> <x0> <y0> <anchoU> <altoU> <escala>   (solo el arte sobre blanco)
static int region_main(char **argv) {
    const char *seed = argv[2], *out = argv[3];
    double x0 = atof(argv[4]), y0 = atof(argv[5]), wu = atof(argv[6]), hu = atof(argv[7]), sc = atof(argv[8]);
    g_skip_invisible = 1;
    { const char *e = getenv("WP_EL_OFF"); if (e) g_el_off = (unsigned)strtoul(e, NULL, 0); }   // pruebas
    prng_seed_str(seed);
    gfx_init();
    world_load(0, WIN_W);
    int W = (int)floor(wu * sc + 0.5), H = (int)floor(hu * sc + 0.5);
    uint8_t *art = malloc((size_t)W * H * 4);
    gfx_render(art, W, H, x0, y0, sc);
    wchar_t wout[512];
    MultiByteToWideChar(CP_UTF8, 0, out, -1, wout, 512);
    int ok = gfx_save_png(art, W, H, wout);
    printf("%s %dx%d\n", ok ? "ok" : "ERROR", W, H);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc >= 9 && !strcmp(argv[1], "--region")) return region_main(argv);
    if (argc >= 4 && !strcmp(argv[1], "--render")) return render_main(argc, argv);
    return wallpaper_run(argc, argv);
}
