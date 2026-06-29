using System;
using System.Buffers.Binary;
using SkiaSharp;

namespace SmartDisplayPcAgent.Services;

public sealed class AnimationImageService
{
    public const int Width = 240;
    public const int Height = 240;
    public const int HeaderBytes = 32;
    public const int PixelBytes = Width * Height * 2;
    public const int PayloadBytes = HeaderBytes + PixelBytes;
    public const int MaxSourceBytes = 16 * 1024 * 1024;

    private const uint Magic = 0x31494D54;
    private const ushort Version = 1;
    private const ushort FormatRgb565LittleEndian = 1;

    public byte[] CreateUploadPayload(byte[] sourceBytes)
    {
        if (sourceBytes.Length == 0)
            throw new InvalidOperationException("Arquivo vazio.");

        if (sourceBytes.Length > MaxSourceBytes)
            throw new InvalidOperationException("Arquivo maior que 16 MB.");

        using var source = SKBitmap.Decode(sourceBytes);
        if (source is null || source.Width <= 0 || source.Height <= 0)
            throw new InvalidOperationException("Formato de imagem não suportado.");

        var imageInfo = new SKImageInfo(Width, Height, SKColorType.Rgba8888, SKAlphaType.Premul);
        using var resized = new SKBitmap(imageInfo);
        using var canvas = new SKCanvas(resized);
        using var paint = new SKPaint
        {
            FilterQuality = SKFilterQuality.High,
            IsAntialias = true
        };

        int side = Math.Min(source.Width, source.Height);
        var sourceRect = new SKRect(
            (source.Width - side) / 2f,
            (source.Height - side) / 2f,
            (source.Width + side) / 2f,
            (source.Height + side) / 2f);

        canvas.Clear(SKColors.Black);
        canvas.DrawBitmap(source, sourceRect, new SKRect(0, 0, Width, Height), paint);
        canvas.Flush();

        byte[] payload = new byte[PayloadBytes];
        int offset = HeaderBytes;
        for (int y = 0; y < Height; y++)
        {
            for (int x = 0; x < Width; x++)
            {
                SKColor pixel = resized.GetPixel(x, y);
                ushort rgb565 = ToRgb565(pixel);
                BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(offset, 2), rgb565);
                offset += 2;
            }
        }

        uint crc = Crc32(payload.AsSpan(HeaderBytes, PixelBytes));
        WriteHeader(payload, crc);
        return payload;
    }

    private static ushort ToRgb565(SKColor color)
    {
        return (ushort)(((color.Red & 0xF8) << 8) |
                        ((color.Green & 0xFC) << 3) |
                        (color.Blue >> 3));
    }

    private static void WriteHeader(byte[] payload, uint crc)
    {
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(0, 4), Magic);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(4, 2), Version);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(6, 2), FormatRgb565LittleEndian);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(8, 2), Width);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(10, 2), Height);
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(12, 4), PixelBytes);
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(16, 4), crc);
    }

    private static uint Crc32(ReadOnlySpan<byte> data)
    {
        uint crc = 0xFFFFFFFF;
        foreach (byte value in data)
        {
            crc ^= value;
            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc >> 1) ^ (0xEDB88320u & (uint)-(int)(crc & 1));
            }
        }
        return ~crc;
    }
}
