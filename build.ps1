# Builds shanshui.exe, nippon.exe and config.exe into .\bin
# Compila shanshui.exe, nippon.exe y config.exe en .\bin
#
# Needs / Necesita: MinGW-w64 gcc + windres (e.g. w64devkit: https://github.com/skeeto/w64devkit)
#   .\build.ps1                       # gcc on PATH, or found in common locations
#   .\build.ps1 -Gcc C:\path\to\bin   # explicit folder with gcc.exe
param([string]$Gcc = "")
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

function Use-Dir($d) { if ($d -and (Test-Path (Join-Path $d "gcc.exe"))) { $env:Path = "$d;$env:Path"; return $true }; return $false }
if (-not (Use-Dir $Gcc) -and -not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    $cands = @("$env:W64DEVKIT\bin", "C:\w64devkit\bin", "$env:USERPROFILE\w64devkit\bin", "C:\dev\w64devkit\bin",
               "C:\msys64\ucrt64\bin", "C:\msys64\mingw64\bin")
    $found = $false
    foreach ($d in $cands) { if (Use-Dir $d) { $found = $true; break } }
    if (-not $found) { throw "gcc not found / no se encontro gcc. Install w64devkit and add its bin folder to PATH, or use -Gcc <folder>." }
}

$bin = Join-Path $root "bin"; $obj = Join-Path $root "build"
New-Item -ItemType Directory -Force $bin, $obj | Out-Null
Push-Location $root
try {
    & windres res.rc -O coff -o "$obj\res.o"
    if ($LASTEXITCODE -ne 0) { throw "windres failed" }
    $wall = @("-O2", "-ffp-contract=off", "-std=gnu11", "-Wall", "-Wno-unused-function", "-mwindows")
    $wlibs = @("-lm", "-lgdiplus", "-lgdi32", "-luser32", "-lshell32")
    foreach ($p in @("shanshui", "nippon")) {
        $srcs = Get-ChildItem "$root\$p\src\*.c" | ForEach-Object { $_.FullName }
        & gcc @wall -o "$bin\$p.exe" @srcs "$obj\res.o" @wlibs
        if ($LASTEXITCODE -ne 0) { throw "$p failed" }
        "OK  bin\$p.exe"
    }
    $srcs = Get-ChildItem "$root\config\src\*.c" | ForEach-Object { $_.FullName }
    $cflags = @("-O2", "-std=gnu11", "-D_WIN32_WINNT=0x0A00", "-DUNICODE", "-Wall", "-Wno-unused-parameter",
                "-Wno-missing-field-initializers", "-mwindows")
    & windres --include-dir . config\config.rc -O coff -o "$obj\config-res.o"   # manifiesto + icono (torii)
    if ($LASTEXITCODE -ne 0) { throw "windres (config) failed" }
    & gcc @cflags -o "$bin\config.exe" @srcs "$obj\config-res.o" -lm -lgdiplus -lgdi32 -luser32 -ladvapi32 -lshell32
    if ($LASTEXITCODE -ne 0) { throw "config failed" }
    "OK  bin\config.exe"
} finally { Pop-Location }
