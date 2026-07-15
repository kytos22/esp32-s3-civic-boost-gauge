param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [string]$OutputBase = "startup_honda",
    [string]$Symbol = "startup_honda_logo",
    [int]$TargetWidth = 320,
    [int]$TargetHeight = 215
)

Add-Type -AssemblyName System.Drawing

$projectRoot = Split-Path -Parent $PSScriptRoot
$assetDirectory = Join-Path $projectRoot "assets"
$pngPath = Join-Path $assetDirectory "$OutputBase.png"
$cPath = Join-Path $projectRoot "src\$OutputBase.c"

New-Item -ItemType Directory -Force -Path $assetDirectory | Out-Null

if ([System.IO.Path]::GetExtension($InputPath) -eq '.svg') {
    & magick -background none $InputPath $pngPath
    if ($LASTEXITCODE -ne 0) { throw "ImageMagick could not rasterize $InputPath" }

    $rendered = [System.Drawing.Bitmap]::FromFile($pngPath)
    $target = New-Object System.Drawing.Bitmap(
        $targetWidth,
        $targetHeight,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($target)
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $graphics.DrawImageUnscaled($rendered, 0, 0)
    $graphics.Dispose()
    $rendered.Dispose()
} else {
    $sourceIndexed = [System.Drawing.Bitmap]::FromFile($InputPath)
    $source = New-Object System.Drawing.Bitmap(
        $sourceIndexed.Width,
        $sourceIndexed.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $sourceGraphics = [System.Drawing.Graphics]::FromImage($source)
    $sourceGraphics.DrawImageUnscaled($sourceIndexed, 0, 0)
    $sourceGraphics.Dispose()
    $sourceIndexed.Dispose()

    # The downloaded PNG contains a baked checkerboard made from these exact colors.
    $source.MakeTransparent([System.Drawing.Color]::FromArgb(255, 230, 230, 230))
    $source.MakeTransparent([System.Drawing.Color]::White)

    $crop = New-Object System.Drawing.Rectangle(200, 75, 520, 350)
    $target = New-Object System.Drawing.Bitmap(
        $targetWidth,
        $targetHeight,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($target)
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $destination = New-Object System.Drawing.Rectangle(0, 0, $targetWidth, $targetHeight)
    $graphics.DrawImage($source, $destination, $crop, [System.Drawing.GraphicsUnit]::Pixel)
    $graphics.Dispose()
    $source.Dispose()
    $target.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
}

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class LvglImageWriter
{
    public static void WriteRgb565Alpha(Bitmap bitmap, string path, string symbol)
    {
        Rectangle rectangle = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(
            rectangle, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        byte[] pixels = new byte[data.Stride * data.Height];
        Marshal.Copy(data.Scan0, pixels, 0, pixels.Length);
        bitmap.UnlockBits(data);

        using (StreamWriter writer = new StreamWriter(path, false, Encoding.ASCII))
        {
            writer.WriteLine("#include <lvgl.h>");
            writer.WriteLine();
            writer.WriteLine("static const uint8_t {0}_map[] = {{", symbol);
            int bytesOnLine = 0;
            for (int y = 0; y < bitmap.Height; ++y)
            {
                for (int x = 0; x < bitmap.Width; ++x)
                {
                    int offset = y * data.Stride + x * 4;
                    int blue = pixels[offset];
                    int green = pixels[offset + 1];
                    int red = pixels[offset + 2];
                    int alpha = pixels[offset + 3];
                    int rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
                    int[] values = { (rgb565 >> 8) & 0xff, rgb565 & 0xff, alpha };
                    foreach (int value in values)
                    {
                        if (bytesOnLine == 0) writer.Write("    ");
                        writer.Write("0x{0:x2},", value);
                        ++bytesOnLine;
                        if (bytesOnLine == 12)
                        {
                            writer.WriteLine();
                            bytesOnLine = 0;
                        }
                        else
                        {
                            writer.Write(' ');
                        }
                    }
                }
            }
            if (bytesOnLine != 0) writer.WriteLine();
            writer.WriteLine("};");
            writer.WriteLine();
            writer.WriteLine("const lv_img_dsc_t {0} = {{", symbol);
            writer.WriteLine("    .header = {");
            writer.WriteLine("        .w = {0},", bitmap.Width);
            writer.WriteLine("        .h = {0},", bitmap.Height);
            writer.WriteLine("        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,");
            writer.WriteLine("    },");
            writer.WriteLine("    .data_size = sizeof({0}_map),", symbol);
            writer.WriteLine("    .data = {0}_map,", symbol);
            writer.WriteLine("};");
        }
    }
}
'@

[LvglImageWriter]::WriteRgb565Alpha($target, $cPath, $Symbol)
$target.Dispose()

Write-Output "Generated $pngPath and $cPath"
