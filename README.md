# Inkdrift

*Endless ink-painted landscapes drifting across your desktop.*
*Paisajes a tinta que derivan sin fin por tu escritorio.*

## ⬇ [Download for Windows / Descargar para Windows](https://github.com/sSagu/inkdrift/releases/latest/download/inkdrift-windows-x64.zip)

1. Download the zip and extract it anywhere. / Bajá el zip y descomprimilo donde quieras.
2. Open **`config.exe`**. / Abrí **`config.exe`**.
3. Pick **CHINA** or **JAPAN** and press **APPLY** (button **ES/EN** for the language). / Elegí **CHINA** o **JAPÓN** y apretá **APLICAR**.

That's it: no installer, no admin rights. Windows may show a SmartScreen warning the first time (the app is not code-signed): *More info → Run anyway*.
Eso es todo: sin instalador ni permisos de administrador. Windows puede mostrar un aviso de SmartScreen la primera vez (no está firmado): *Más información → Ejecutar de todas formas*.

---

Animated, infinitely scrolling ink-painting wallpapers for Windows, written in plain C (no browser, no runtime).
Fondos de pantalla animados de pintura a tinta con scroll infinito para Windows, escritos en C puro (sin navegador).

- **shanshui** — faithful C port of [shan-shui-inf](https://github.com/LingDong-/shan-shui-inf) by Lingdong Huang (Chinese landscape). Same seed → same landscape as the website.
- **nippon** — Japanese variant: Fuji, torii, pagodas, bridges, villages, samurai, koi, bamboo, cherry blossoms, stylized clouds and small waves, subtle faded color.
- **config** — settings window in hanging-scroll style (English / Spanish) to choose the wallpaper, toggle elements, speed, distance and startup.

---

## English

### Build from source (optional)
1. Install [w64devkit](https://github.com/skeeto/w64devkit) (or any MinGW-w64 with `gcc` and `windres`).
2. In PowerShell:
   ```powershell
   .\build.ps1                 # or: .\build.ps1 -Gcc C:\path\to\w64devkit\bin
   ```
   Output: `bin\shanshui.exe`, `bin\nippon.exe`, `bin\config.exe` (keep them in the same folder).

### Use
- Run `bin\config.exe`. Use the **ES/EN** button for English. Pick **CHINA** or **JAPAN**, toggle elements and press **APPLY**. It starts the chosen wallpaper and, if "START WITH WINDOWS" is on, registers it to start with Windows (current user only, no admin).
- Or run `bin\shanshui.exe` / `bin\nippon.exe` directly. Tray icon: pause, new landscape, exit.
- **DISTANCE** moves the camera: lower values show more of the world and more sky, higher values bring you into the foreground.
- The wallpaper pauses itself when a window covers the screen. Run it from a normal (non-admin) session.

### Settings file
`%APPDATA%\FondoShanShui\config.ini` (written by `config.exe`):
`lang=es|en`, `fondo=shanshui|nippon`, `seed=` (empty = random each start), `speed`, `fps`, `zoom`, `pause_when_covered`, `start_with_windows`,
and per-element switches `cn.<element>=0/1` / `jp.<element>=0/1`.
Command line overrides: `--seed`, `--speed`, `--fps`, `--zoom`, `--nopause`, `--config <file>`.

### Uninstall
Hold **LEAVE** in `config.exe` or exit from the tray, turn off "START WITH WINDOWS", then delete the folder and `%APPDATA%\FondoShanShui`.

---

## Español

### Compilar desde el código (opcional)
1. Instalá [w64devkit](https://github.com/skeeto/w64devkit) (o cualquier MinGW-w64 con `gcc` y `windres`).
2. En PowerShell:
   ```powershell
   .\build.ps1                 # o: .\build.ps1 -Gcc C:\ruta\a\w64devkit\bin
   ```
   Resultado: `bin\shanshui.exe`, `bin\nippon.exe`, `bin\config.exe` (tienen que quedar en la misma carpeta).

### Uso
- Abrí `bin\config.exe` (botón **ES/EN** para cambiar el idioma), elegí **CHINA** o **JAPÓN**, prendé/apagá elementos y apretá **APLICAR**. Arranca el fondo elegido y, si "DESPERTAR CON WINDOWS" está activo, lo registra para iniciar con Windows (solo tu usuario, sin admin).
- O ejecutá `bin\shanshui.exe` / `bin\nippon.exe` directamente. Icono en la bandeja: pausar, nuevo paisaje, salir.
- **CERCANÍA** mueve la cámara: valores bajos muestran más mundo y más cielo; valores altos te meten en el primer plano.
- El fondo se pausa solo cuando una ventana tapa la pantalla. Ejecutalo sin permisos de administrador.

### Archivo de configuración
`%APPDATA%\FondoShanShui\config.ini` (lo escribe `config.exe`):
`lang=es|en`, `fondo=shanshui|nippon`, `seed=` (vacío = al azar en cada arranque), `speed`, `fps`, `zoom`, `pause_when_covered`, `start_with_windows`,
y un interruptor por elemento `cn.<elemento>=0/1` / `jp.<elemento>=0/1`.
Línea de comandos: `--seed`, `--speed`, `--fps`, `--zoom`, `--nopause`, `--config <archivo>`.

### Desinstalar
Mantené apretado **RETIRARSE** en `config.exe` o salí desde la bandeja, desactivá "DESPERTAR CON WINDOWS" y borrá la carpeta y `%APPDATA%\FondoShanShui`.

---

License: MIT (see `LICENSE`). Original algorithm and art direction © Lingdong Huang.
