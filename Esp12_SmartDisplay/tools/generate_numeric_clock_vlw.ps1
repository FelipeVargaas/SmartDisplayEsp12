param(
  [string]$Output = (Join-Path $PSScriptRoot '..\data\numeric_clock.vlw'),
  [string]$FontPath = 'C:\Windows\Fonts\bahnschrift.ttf',
  [int]$SizePx = 52
)

Add-Type -AssemblyName System.Drawing

$chars = '0123456789:%C'.ToCharArray()
$style = [System.Drawing.FontStyle]::Bold
$privateFonts = New-Object System.Drawing.Text.PrivateFontCollection
$privateFonts.AddFontFile($FontPath)
$family = $privateFonts.Families[0]
$font = New-Object System.Drawing.Font($family, [single]$SizePx, $style, [System.Drawing.GraphicsUnit]::Pixel)
$format = [System.Drawing.StringFormat]::GenericTypographic
$format.FormatFlags = $format.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces

$emHeight = $family.GetEmHeight($style)
$ascent = [int][Math]::Ceiling($family.GetCellAscent($style) * $font.Size / $emHeight)
$descent = [int][Math]::Ceiling($family.GetCellDescent($style) * $font.Size / $emHeight)

function Add-Int32BE([System.Collections.Generic.List[byte]]$bytes, [int]$value) {
  $u = [uint32]$value
  $bytes.Add([byte](($u -shr 24) -band 0xFF))
  $bytes.Add([byte](($u -shr 16) -band 0xFF))
  $bytes.Add([byte](($u -shr 8) -band 0xFF))
  $bytes.Add([byte]($u -band 0xFF))
}

function Measure-Glyph([char]$ch) {
  $bmp = New-Object System.Drawing.Bitmap 96, 80, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawString([string]$ch, $font, [System.Drawing.Brushes]::White, 0, 0, $format)
  $advance = [int][Math]::Ceiling($g.MeasureString([string]$ch, $font, 200, $format).Width)
  $g.Dispose()

  $minX = $bmp.Width
  $minY = $bmp.Height
  $maxX = -1
  $maxY = -1
  for ($y = 0; $y -lt $bmp.Height; $y++) {
    for ($x = 0; $x -lt $bmp.Width; $x++) {
      if ($bmp.GetPixel($x, $y).A -gt 0) {
        if ($x -lt $minX) { $minX = $x }
        if ($y -lt $minY) { $minY = $y }
        if ($x -gt $maxX) { $maxX = $x }
        if ($y -gt $maxY) { $maxY = $y }
      }
    }
  }

  if ($maxX -lt 0) {
    $bmp.Dispose()
    return [pscustomobject]@{ Width = 1; Height = 1; Advance = 6; DY = $ascent; DX = 0; Alpha = [byte[]](0) }
  }

  $w = $maxX - $minX + 1
  $h = $maxY - $minY + 1
  $alpha = New-Object byte[] ($w * $h)
  for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
      $alpha[$y * $w + $x] = $bmp.GetPixel($minX + $x, $minY + $y).A
    }
  }

  if ($advance -lt ($w + 2)) { $advance = $w + 2 }
  $bmp.Dispose()
  return [pscustomobject]@{ Width = $w; Height = $h; Advance = $advance; DY = $ascent - $minY; DX = 0; Alpha = $alpha }
}

$glyphs = @()
foreach ($ch in $chars) {
  $glyphs += [pscustomobject]@{ Char = $ch; Code = [int][char]$ch; Glyph = (Measure-Glyph $ch) }
}

$bytes = New-Object 'System.Collections.Generic.List[byte]'
Add-Int32BE $bytes $glyphs.Count
Add-Int32BE $bytes 11
Add-Int32BE $bytes $SizePx
Add-Int32BE $bytes 0
Add-Int32BE $bytes $ascent
Add-Int32BE $bytes $descent

foreach ($item in $glyphs) {
  Add-Int32BE $bytes $item.Code
  Add-Int32BE $bytes $item.Glyph.Height
  Add-Int32BE $bytes $item.Glyph.Width
  Add-Int32BE $bytes $item.Glyph.Advance
  Add-Int32BE $bytes $item.Glyph.DY
  Add-Int32BE $bytes $item.Glyph.DX
  Add-Int32BE $bytes 0
}

foreach ($item in $glyphs) {
  $bytes.AddRange($item.Glyph.Alpha)
}

$fontName = [System.Text.Encoding]::ASCII.GetBytes('TinyDashNumeric')
$postscriptName = [System.Text.Encoding]::ASCII.GetBytes('TinyDashNumeric')
$bytes.Add([byte]$fontName.Length)
$bytes.AddRange($fontName)
$bytes.Add([byte]0)
$bytes.Add([byte]$postscriptName.Length)
$bytes.AddRange($postscriptName)
$bytes.Add([byte]0)
$bytes.Add([byte]1)

$outputDir = Split-Path -Parent $Output
if (!(Test-Path -LiteralPath $outputDir)) {
  New-Item -ItemType Directory -Path $outputDir | Out-Null
}

[System.IO.File]::WriteAllBytes((Resolve-Path -LiteralPath $outputDir).Path + '\' + (Split-Path -Leaf $Output), $bytes.ToArray())
Write-Host "Generated $Output ($($bytes.Count) bytes)"
