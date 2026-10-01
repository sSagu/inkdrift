# Genera config\icon.ico: un torii en pixel art (16x16) escalado sin suavizado a 16..256 px.
# Uso: powershell -ExecutionPolicy Bypass -File config\make-icon.ps1
Add-Type -AssemblyName System.Drawing
$grid = @(
    ".WWWWWWWWWWWWWW.",
    "WPPPPPPPPPPPPPPW",
    "WKPPPPPPPPPPPPKW",
    "WKKKKKKKKKKKKKKW",
    "WPRRRRRRRRRRRRPW",
    "WPPRRPPRRPPRRPPW",
    "WPRRRRRRRRRRRRPW",
    "WPPRRPPPPPPRRPPW",
    "WPPRRPPPPPPRRPPW",
    "WPPRRPPPPPPRRPPW",
    "WPPRRPPPPPPRRPPW",
    "WPPRRPPPPPPRRPPW",
    "WPPRRPPPPPPRRPPW",
    "WPKKKKPPPPKKKKPW",
    "WPPPPPPPPPPPPPPW",
    ".WWWWWWWWWWWWWW."
)
$col = @{
    'K' = [System.Drawing.Color]::FromArgb(255, 58, 42, 34)     # tinta
    'R' = [System.Drawing.Color]::FromArgb(255, 181, 67, 47)    # bermellon de sello
    'W' = [System.Drawing.Color]::FromArgb(255, 74, 46, 31)     # marco de madera
    'P' = [System.Drawing.Color]::FromArgb(255, 233, 223, 200)  # papel de arroz
}
$paper = [System.Drawing.Color]::FromArgb(255, 233, 223, 200)
$rim = [System.Drawing.Color]::FromArgb(255, 74, 46, 31)

function Make-Png([int]$size) {
    $s = $size / 16
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    for ($y = 0; $y -lt $size; $y++) {
        for ($x = 0; $x -lt $size; $x++) {
            $gx = [Math]::Floor($x / $s); $gy = [Math]::Floor($y / $s)
            $c = $grid[$gy][$gx]
            $px = [System.Drawing.Color]::Transparent
            if ($col.ContainsKey([string]$c)) { $px = $col[[string]$c] }
            $bmp.SetPixel($x, $y, $px)
        }
    }
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return , $ms.ToArray()
}

$sizes = 16, 32, 48, 64, 128, 256
$pngs = @(); foreach ($sz in $sizes) { $pngs += , (Make-Png $sz) }
$out = Join-Path $PSScriptRoot "icon.ico"
$fs = [System.IO.File]::Create($out); $bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $sz = $sizes[$i]; $b = if ($sz -ge 256) { 0 } else { $sz }
    $bw.Write([byte]$b); $bw.Write([byte]$b); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32); $bw.Write([UInt32]$pngs[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Close()
"icono: $out ($((Get-Item $out).Length) bytes)"
