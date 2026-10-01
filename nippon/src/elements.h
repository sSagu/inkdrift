// Elementos del fondo que se pueden apagar desde config.ini con "jp.<nombre>=0".
#ifndef ELEMENTS_H
#define ELEMENTS_H
enum { EL_TREES, EL_ROCKS, EL_BUILDINGS, EL_SHRINES, EL_BOATS, EL_WATER, EL_DISTANT, EL_FUJI, EL_FLAT, EL_KOI, EL_BRIDGES, EL_SAMURAI, EL_VILLAGES, EL_BAMBOO, EL_SAKURA, EL_GREEN, EL_EASTER, EL_CLOUDS, EL_WAVES, EL_COUNT };
extern const char *const EL_PREFIX;              // "jp"
extern const char *const EL_NAMES[EL_COUNT];
#endif