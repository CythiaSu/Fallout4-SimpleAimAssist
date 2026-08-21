param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "Assets\Enhanced")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function New-Color([int]$alpha, [int]$red, [int]$green, [int]$blue) {
    [System.Drawing.Color]::FromArgb($alpha, $red, $green, $blue)
}

function New-Canvas {
    $bitmap = [System.Drawing.Bitmap]::new(
        256,
        256,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
    $graphics.Clear((New-Color 0 0 0 0))
    return @{ Bitmap = $bitmap; Graphics = $graphics }
}

function New-Points([object[]]$values) {
    $points = [System.Drawing.PointF[]]::new($values.Count / 2)
    for ($i = 0; $i -lt $values.Count; $i += 2) {
        $points[$i / 2] = [System.Drawing.PointF]::new(
            [float]$values[$i],
            [float]$values[$i + 1])
    }
    return $points
}

function Draw-GlowLine($graphics, [System.Drawing.PointF[]]$points, [int[]]$rgb, [float]$width = 2.0) {
    $main = [System.Drawing.Pen]::new((New-Color 245 $rgb[0] $rgb[1] $rgb[2]), $width)
    $graphics.DrawLines($main, $points)
    $main.Dispose()
}

function Draw-GlowPolygon($graphics, [System.Drawing.PointF[]]$points, [int[]]$rgb, [bool]$filled = $true) {
    if ($filled) {
        $brush = [System.Drawing.SolidBrush]::new((New-Color 245 $rgb[0] $rgb[1] $rgb[2]))
        $graphics.FillPolygon($brush, $points)
        $brush.Dispose()
    }
    $outline = [System.Drawing.Pen]::new((New-Color 248 $rgb[0] $rgb[1] $rgb[2]), 1.8)
    $graphics.DrawPolygon($outline, $points)
    $outline.Dispose()
}

function Save-Canvas($canvas, [string]$name) {
    $path = Join-Path $OutputDirectory $name
    $canvas.Graphics.Dispose()
    $canvas.Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.Bitmap.Dispose()
    Get-Item -LiteralPath $path | Select-Object FullName, Length, LastWriteTime
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Get-ChildItem -LiteralPath $OutputDirectory -Filter "AimMarker_*.png" -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

$red = @(238, 28, 48)
$white = @(248, 250, 252)

# 1. Approved compact red downward triangle.
$canvas = New-Canvas
$triangle = New-Points @(67, 56, 189, 56, 128, 162)
Draw-GlowPolygon $canvas.Graphics $triangle $red $true
Save-Canvas $canvas "AimMarker_RedTriangle_Enhanced.png"

# 2. Clean monochrome reticle. This stays deliberately simple and white.
$canvas = New-Canvas
Draw-GlowLine $canvas.Graphics (New-Points @(42, 70, 94, 70, 105, 82)) $white 3.0
Draw-GlowLine $canvas.Graphics (New-Points @(214, 70, 162, 70, 151, 82)) $white 3.0
Draw-GlowLine $canvas.Graphics (New-Points @(70, 91, 186, 91)) $white 2.4
Draw-GlowLine $canvas.Graphics (New-Points @(90, 112, 166, 112)) $white 2.4
Draw-GlowLine $canvas.Graphics (New-Points @(103, 132, 128, 157, 153, 132)) $white 3.2
Save-Canvas $canvas "AimMarker_SimpleWhite_Enhanced.png"
