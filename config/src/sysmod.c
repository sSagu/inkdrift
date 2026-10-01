// sysmod: clave Run y mensajes a fondo.exe (ver sysmod.h).
#include "sysmod.h"
#include "cfg.h"
#include <wchar.h>

static void w2u(const wchar_t *w, char *o, int n) { WideCharToMultiByte(CP_UTF8, 0, w, -1, o, n, NULL, NULL); }

int fc_variant = 0;
static const wchar_t *const CLS[2] = {L"FondoShanShuiCtl", L"NipponCtl"};

int fc_fondo_exe(wchar_t *out, int n) {
    wchar_t me[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, me, MAX_PATH);
    if (!len || len >= MAX_PATH) return 0;
    wchar_t *s = wcsrchr(me, L'\\');
    if (!s) return 0;
    s[1] = 0;
    wchar_t raw[MAX_PATH * 2];
    int r = _snwprintf(raw, MAX_PATH * 2, fc_variant ? L"%lsnippon.exe" : L"%lsshanshui.exe", me);
    if (r <= 0 || r >= MAX_PATH * 2) return 0;
    DWORD m = GetFullPathNameW(raw, (DWORD)n, out, NULL);   // sin ".." en la clave Run
    return m > 0 && (int)m < n;
}

int fc_startup_command(wchar_t *out, int n) {
    wchar_t exe[MAX_PATH];
    if (!fc_fondo_exe(exe, MAX_PATH)) return 0;
    int r = _snwprintf(out, (size_t)n, L"\"%ls\"", exe);
    return r > 0 && r < n;
}

int fc_startup_query(const wchar_t *name, const wchar_t *expected, wchar_t *data, int n) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, FC_RUN_KEY, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return FC_RUN_ABSENT;
    wchar_t buf[1024];
    DWORD type = 0, sz = sizeof buf - sizeof(wchar_t);
    LONG r = RegQueryValueExW(k, name, NULL, &type, (BYTE *)buf, &sz);
    RegCloseKey(k);
    if (r == ERROR_FILE_NOT_FOUND) return FC_RUN_ABSENT;
    if (r != ERROR_SUCCESS) return FC_RUN_ERROR;
    buf[sz / sizeof(wchar_t)] = 0;
    if (data && n > 0) { wcsncpy(data, buf, (size_t)n - 1); data[n - 1] = 0; }
    if ((type == REG_SZ || type == REG_EXPAND_SZ) && expected && !_wcsicmp(buf, expected)) return FC_RUN_MATCH;
    return FC_RUN_OTHER;
}

int fc_startup_set(const wchar_t *name, const wchar_t *data, int dry) {
    char n8[256], d8[1024];
    if (dry) { w2u(name, n8, 256); w2u(data, d8, 1024); fc_log("[dry-run] HKCU\\...\\Run: %s = %s", n8, d8); return 1; }
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, FC_RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return 0;
    LONG r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)data, (DWORD)((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

int fc_startup_delete(const wchar_t *name, int dry) {
    char n8[256];
    if (dry) { w2u(name, n8, 256); fc_log("[dry-run] HKCU\\...\\Run: borrar %s", n8); return 1; }
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, FC_RUN_KEY, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return 1;
    LONG r = RegDeleteValueW(k, name);
    RegCloseKey(k);
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

int fc_startup_apply(const wchar_t *name, int on, int dry) {
    if (!on) return fc_startup_delete(name, dry);
    wchar_t cmd[MAX_PATH + 4];
    if (!fc_startup_command(cmd, MAX_PATH + 4)) return 0;
    if (!dry && fc_startup_query(name, cmd, NULL, 0) == FC_RUN_MATCH) return 1;
    return fc_startup_set(name, cmd, dry);
}

HWND fc_fondo_find_v(int v) { return FindWindowW(CLS[v ? 1 : 0], NULL); }
HWND fc_fondo_find(void) { return fc_fondo_find_v(fc_variant); }
int fc_fondo_quit_v(int v, int dry) {
    HWND h = fc_fondo_find_v(v);
    if (dry) { fc_log("[dry-run] WM_COMMAND 1 (salir) -> fondo %d (%s)", v, h ? "corriendo" : "no corre"); return h != NULL; }
    return h && PostMessageW(h, WM_COMMAND, 1, 0);
}

static int post(UINT msg, WPARAM wp, int dry, const char *what) {
    HWND h = fc_fondo_find();
    if (dry) { fc_log("[dry-run] %s -> FondoShanShuiCtl (%s)", what, h ? "corriendo" : "no corre"); return h != NULL; }
    return h && PostMessageW(h, msg, wp, 0);
}
int fc_fondo_reload(int dry) { return post(FC_WM_RELOAD, 0, dry, "WM_APP+10 (recargar)"); }
int fc_fondo_new_landscape(int dry) { return post(WM_COMMAND, 3, dry, "WM_COMMAND 3 (nuevo paisaje)"); }
int fc_fondo_quit(int dry) { return post(WM_COMMAND, 1, dry, "WM_COMMAND 1 (salir)"); }

int fc_fondo_launch(int dry) {
    wchar_t exe[MAX_PATH], dir[MAX_PATH], cmd[MAX_PATH + 4];
    if (!fc_fondo_exe(exe, MAX_PATH)) return 0;
    if (dry) { char e8[MAX_PATH * 3]; w2u(exe, e8, sizeof e8); fc_log("[dry-run] CreateProcess %s", e8); return 1; }
    wcscpy(dir, exe);
    *wcsrchr(dir, L'\\') = 0;
    _snwprintf(cmd, MAX_PATH + 4, L"\"%ls\"", exe);
    cmd[MAX_PATH + 3] = 0;
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}

int fc_fondo_reload_or_launch(int dry) {
    if (fc_fondo_find()) return fc_fondo_reload(dry);
    return fc_fondo_launch(dry);
}
