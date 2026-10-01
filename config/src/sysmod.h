// sysmod: inicio con Windows (clave Run de HKCU) y comunicacion con shanshui.exe.
// Todas las funciones que cambian algo aceptan dry_run: en ese caso solo describen por stderr.
#ifndef SYSMOD_H
#define SYSMOD_H
#include <windows.h>

#define FC_RUN_VALUE L"FondoShanShui"
#define FC_RUN_KEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

// ---- startup
enum { FC_RUN_ERROR = -1, FC_RUN_ABSENT = 0, FC_RUN_MATCH = 1, FC_RUN_OTHER = 2 };
// Ruta de shanshui.exe: misma carpeta que el exe actual. 1 = ok.
int fc_fondo_exe(wchar_t *out, int n);
// Fondo con el que se trabaja: 0 = shanshui.exe (China, misma carpeta), 1 = nippon.exe (Japon)
extern int fc_variant;
HWND fc_fondo_find_v(int v);
int fc_fondo_quit_v(int v, int dry_run);
// Dato para la clave Run: la ruta de shanshui.exe entre comillas.
int fc_startup_command(wchar_t *out, int n);
// Estado del valor 'name': ausente / igual a 'expected' / con otro dato. Si data != NULL, copia el dato.
int fc_startup_query(const wchar_t *name, const wchar_t *expected, wchar_t *data, int n);
int fc_startup_set(const wchar_t *name, const wchar_t *data, int dry_run);   // REG_SZ; 1 = ok
int fc_startup_delete(const wchar_t *name, int dry_run);                     // 1 = ok (ausente tambien)
// Deja el registro igual que el toggle (idempotente): on -> set con fc_startup_command, off -> delete.
int fc_startup_apply(const wchar_t *name, int on, int dry_run);

// ---- fondo (ventana de control "FondoShanShuiCtl")
#define FC_WM_RELOAD (WM_APP + 10)
HWND fc_fondo_find(void);
int fc_fondo_reload(int dry_run);         // WM_APP+10. 1 = enviado, 0 = no corre
int fc_fondo_new_landscape(int dry_run);  // WM_COMMAND 3
int fc_fondo_quit(int dry_run);           // WM_COMMAND 1
int fc_fondo_launch(int dry_run);         // CreateProcess (sin elevar). 1 = ok
int fc_fondo_reload_or_launch(int dry_run);

#endif
