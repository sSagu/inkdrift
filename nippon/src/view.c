#include "view.h"

PaperTile paper_scale(const uint8_t *src, double kappa) {
    PaperTile t;
    t.size = (int)floor(512 * kappa + 0.5);
    if (t.size < 64) t.size = 64;
    t.rgb = malloc((size_t)t.size * t.size * 3);
    double k = 512.0 / t.size;
    for (int y = 0; y < t.size; y++) {
        double sy = (y + 0.5) * k - 0.5;
        int y0 = (int)floor(sy);
        double fy = sy - y0;
        int ya = ((y0 % 512) + 512) % 512, yb = (ya + 1) % 512;
        for (int x = 0; x < t.size; x++) {
            double sx = (x + 0.5) * k - 0.5;
            int x0 = (int)floor(sx);
            double fx = sx - x0;
            int xa = ((x0 % 512) + 512) % 512, xb = (xa + 1) % 512;
            for (int c = 0; c < 3; c++) {
                double v00 = src[((size_t)ya * 512 + xa) * 3 + c], v10 = src[((size_t)ya * 512 + xb) * 3 + c];
                double v01 = src[((size_t)yb * 512 + xa) * 3 + c], v11 = src[((size_t)yb * 512 + xb) * 3 + c];
                double v = (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
                t.rgb[((size_t)y * t.size + x) * 3 + c] = (uint8_t)(v + 0.5);
            }
        }
    }
    return t;
}

void multiply_rows(uint8_t *dst, const uint8_t *art, int w, int row0, int rows, const PaperTile *p) {
    for (int r = 0; r < rows; r++) {
        int y = row0 + r;
        const uint8_t *prow = p->rgb + (size_t)(y % p->size) * p->size * 3;
        const uint8_t *a = art + (size_t)r * w * 4;
        uint8_t *d = dst + (size_t)r * w * 4;
        int px = 0;
        for (int x = 0; x < w; x++) {
            const uint8_t *pp = prow + (size_t)px * 3;
            // BGRA: papel es RGB
            unsigned t0 = a[0] * pp[2] + 128, t1 = a[1] * pp[1] + 128, t2 = a[2] * pp[0] + 128;
            d[0] = (uint8_t)((t0 + (t0 >> 8)) >> 8);
            d[1] = (uint8_t)((t1 + (t1 >> 8)) >> 8);
            d[2] = (uint8_t)((t2 + (t2 >> 8)) >> 8);
            d[3] = 255;
            a += 4; d += 4;
            if (++px == p->size) px = 0;
        }
    }
}
