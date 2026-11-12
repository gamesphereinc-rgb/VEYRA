[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputPath)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$size = 512
$bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.Clear([System.Drawing.Color]::Transparent)

$tilePath = [System.Drawing.Drawing2D.GraphicsPath]::new()
$tileRect = [System.Drawing.Rectangle]::new(14,14,484,484)
$d = 112
[void]$tilePath.AddArc(14,14,$d,$d,180,90)
[void]$tilePath.AddArc(498-$d,14,$d,$d,270,90)
[void]$tilePath.AddArc(498-$d,498-$d,$d,$d,0,90)
[void]$tilePath.AddArc(14,498-$d,$d,$d,90,90)
$tilePath.CloseFigure()
$tileBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
    $tileRect,
    [System.Drawing.Color]::FromArgb(255,8,19,34),
    [System.Drawing.Color]::FromArgb(255,22,39,63),
    135.0
)
$graphics.FillPath($tileBrush,$tilePath)
$borderPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(105,102,190,196),3)
$graphics.DrawPath($borderPen,$tilePath)

$points = [System.Drawing.Point[]]@(
    [System.Drawing.Point]::new(148,126),
    [System.Drawing.Point]::new(256,378),
    [System.Drawing.Point]::new(364,126)
)
$mark = [System.Drawing.Drawing2D.GraphicsPath]::new()
$mark.AddLines($points)
$halo = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(88,67,220,198),68)
$halo.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$halo.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$halo.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
$graphics.DrawPath($halo,$mark)
$gradient = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
    [System.Drawing.Rectangle]::new(120,110,272,290),
    [System.Drawing.Color]::FromArgb(255,109,239,205),
    [System.Drawing.Color]::FromArgb(255,137,166,255),
    90.0
)
$pen = [System.Drawing.Pen]::new($gradient,43)
$pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
$graphics.DrawPath($pen,$mark)

$frames = [System.Collections.Generic.List[object]]::new()
foreach($iconSize in @(16,24,32,48,64,128,256)){
    $scaled = [System.Drawing.Bitmap]::new($bitmap,$iconSize,$iconSize)
    $stream = [System.IO.MemoryStream]::new()
    $scaled.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    $frames.Add([pscustomobject]@{Size=$iconSize;Bytes=$stream.ToArray();Bitmap=$scaled;Stream=$stream})
}

$memory = [System.IO.MemoryStream]::new()
$writer = [System.IO.BinaryWriter]::new($memory)
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$frames.Count)
$offset = 6 + 16*$frames.Count
foreach($frame in $frames){
    $dimension = if($frame.Size -eq 256){[byte]0}else{[byte]$frame.Size}
    $writer.Write($dimension)
    $writer.Write($dimension)
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]32)
    $writer.Write([UInt32]$frame.Bytes.Length)
    $writer.Write([UInt32]$offset)
    $offset += $frame.Bytes.Length
}
foreach($frame in $frames){$writer.Write([byte[]]$frame.Bytes)}
$writer.Flush()
$parent=Split-Path -Parent $OutputPath
if($parent){New-Item -ItemType Directory -Force -Path $parent|Out-Null}
[System.IO.File]::WriteAllBytes($OutputPath,$memory.ToArray())
foreach($frame in $frames){$frame.Bitmap.Dispose();$frame.Stream.Dispose()}
$writer.Dispose();$memory.Dispose()
$pen.Dispose();$gradient.Dispose();$halo.Dispose();$mark.Dispose()
$borderPen.Dispose();$tileBrush.Dispose();$tilePath.Dispose()
$graphics.Dispose();$bitmap.Dispose()
Write-Host "Generated multi-resolution VEYRA icon: $OutputPath"
