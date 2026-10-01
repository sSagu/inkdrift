// Textura de papel del fondo (el <canvas id="bgcanv"> del original).
// Importante: consume numeros aleatorios, y en la web eso ocurre justo despues de la
// primera carga de trozos, asi que hay que llamarla en el mismo punto.
#include "paper.h"

static int clamp255(double v) {
    int r = (int)toFixedN(v, 0);
    return r < 0 ? 0 : (r > 255 ? 255 : r);
}

void paper_make(uint8_t *rgb) {
    const int reso = 512;
    for (int i = 0; i < reso / 2 + 1; i++) {
        for (int j = 0; j < reso / 2 + 1; j++) {
            double c = 245 + noise3(i * 0.1, j * 0.1, 0) * 10;
            c -= rnd() * 20;
            int r = clamp255(c), g = clamp255(c * 0.95), b = clamp255(c * 0.85);
            int xs[2] = {i, reso - i}, ys[2] = {j, reso - j};
            for (int a = 0; a < 2; a++)
                for (int d = 0; d < 2; d++) {
                    int x = xs[a], y = ys[d];
                    if (x < 0 || x >= reso || y < 0 || y >= reso) continue;
                    uint8_t *p = rgb + ((size_t)y * reso + x) * 3;
                    p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b;
                }
        }
    }
}
