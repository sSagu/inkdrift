// Estado del mundo (MEM del original) y carga de trozos.
#ifndef PLAN_H
#define PLAN_H
#include "core.h"

typedef struct {
    Chunk **chunks;     // ordenados por y (orden de pintado)
    int n, cap;
    double xmin, xmax;
    double cwid;
} World;

extern World g_world;
void world_load(double xmin, double xmax);   // chunkloader(xmin, xmax)
void world_reset(void);                      // como recargar la pagina (nueva semilla)
void world_prune(double keep_x);             // libera trozos que quedaron muy a la izquierda

void gen_scroll_test(FILE *out, int steps);

#endif
