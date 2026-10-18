# Makes the mod icons from a round badge image on a white background: cuts out the circle, makes the corners
# transparent and writes Icon128.png (the plugin icon) and Icon512.png (the ficsit.app logo) to OutDir.
#
# From the repository root:
#   powershell -ExecutionPolicy Bypass -File docs\icon\make-icon.ps1 -Source docs\icon\IconSource.jpg -OutDir docs\icon
#   copy docs\icon\Icon128.png Resources\Icon128.png
param([string]$Source, [string]$OutDir)
Add-Type -AssemblyName System.Drawing

$src = [System.Drawing.Bitmap]::FromFile($Source)
$w = $src.Width; $h = $src.Height

# Find the badge's outer edge: first clearly non-white pixel along the middle row and column
function IsBackground($c) { return ($c.R -gt 235 -and $c.G -gt 235 -and $c.B -gt 235) }
$midY = [int]($h / 2); $midX = [int]($w / 2)
$left = 0;   while ($left -lt $midX -and (IsBackground $src.GetPixel($left, $midY))) { $left++ }
$right = $w - 1; while ($right -gt $midX -and (IsBackground $src.GetPixel($right, $midY))) { $right-- }
$top = 0;    while ($top -lt $midY -and (IsBackground $src.GetPixel($midX, $top))) { $top++ }
$bottom = $h - 1; while ($bottom -gt $midY -and (IsBackground $src.GetPixel($midX, $bottom))) { $bottom-- }
$cx = ($left + $right) / 2.0; $cy = ($top + $bottom) / 2.0
$r = [Math]::Min($right - $left, $bottom - $top) / 2.0
Write-Output ("badge: center ({0:N1}, {1:N1}), radius {2:N1} px, in a {3}x{4} image" -f $cx, $cy, $r, $w, $h)

# Square crop around the circle, everything outside it transparent, with an anti-aliased edge
function Save-Icon([int]$size, [string]$path) {
    $dst = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $scale = $size / (2.0 * $r)
    $brush = New-Object System.Drawing.TextureBrush $src
    $brush.WrapMode = [System.Drawing.Drawing2D.WrapMode]::Clamp
    $brush.ScaleTransform($scale, $scale)
    $brush.TranslateTransform(-($cx - $r), -($cy - $r), [System.Drawing.Drawing2D.MatrixOrder]::Prepend)
    # Inset half a pixel so no white fringe from the source survives at the edge
    $g.FillEllipse($brush, 0.5, 0.5, $size - 1.0, $size - 1.0)
    $brush.Dispose(); $g.Dispose()
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
    Write-Output "wrote $path"
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
Save-Icon 128 (Join-Path $OutDir "Icon128.png")
Save-Icon 512 (Join-Path $OutDir "Icon512.png")
$src.Dispose()
