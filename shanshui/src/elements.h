// Elementos del fondo que se pueden apagar desde config.ini con "cn.<nombre>=0".
#ifndef ELEMENTS_H
#define ELEMENTS_H
enum { EL_TREES, EL_ROCKS, EL_BUILDINGS, EL_TOWERS, EL_BOATS, EL_WATER, EL_DISTANT, EL_FLAT, EL_COUNT };
extern const char *const EL_PREFIX;              // "cn"
extern const char *const EL_NAMES[EL_COUNT];
#endif