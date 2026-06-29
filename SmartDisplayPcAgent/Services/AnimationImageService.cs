using SkiaSharp;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;

namespace SmartDisplayPcAgent.Services;

public sealed record AnimationImagePayload(byte[] Payload, int FrameCount, int DelayMs);
public sealed record AnimationImageFraming(double Zoom, double OffsetX, double OffsetY);
public sealed record AnimationImagePreview(byte[][] Frames, int DelayMs);

public sealed class AnimationImageService
{
    public const int Width = 240;
    public const int Height = 240;
    public const int HeaderBytes = 32;
    public const int PixelBytes = Width * Height * 2;
    public const int MaxFrames = 8;
    public const int PayloadBytes = HeaderBytes + PixelBytes;
    public const int MaxPayloadBytes = HeaderBytes + PixelBytes * MaxFrames;
    public const int MaxSourceBytes = 16 * 1024 * 1024;

    private const uint ImageMagic = 0x31494D54;
    private const uint AnimationMagic = 0x31414D54;
    private const ushort Version = 1;
    private const ushort FormatRgb565LittleEndian = 1;
    private const int DefaultFrameDelayMs = 150;
    private const int MinFrameDelayMs = 100;
    private const int MaxFrameDelayMs = 1000;

    public AnimationImagePayload CreateUploadPayload(byte[] sourceBytes, AnimationImageFraming? framing = null)
    {
        if (sourceBytes.Length == 0)
            throw new InvalidOperationException("Arquivo vazio.");

        if (sourceBytes.Length > MaxSourceBytes)
            throw new InvalidOperationException("Arquivo maior que 16 MB.");

        var animationPayload = TryCreateAnimatedUploadPayload(sourceBytes, framing);
        if (animationPayload is not null)
            return animationPayload;

        using var source = SKBitmap.Decode(sourceBytes);
        if (source is null || source.Width <= 0 || source.Height <= 0)
            throw new InvalidOperationException("Formato de imagem nao suportado.");

        using var resized = ResizeToDisplay(source, framing);
        byte[] payload = new byte[PayloadBytes];
        WriteBitmapPixels(resized, payload.AsSpan(HeaderBytes, PixelBytes));

        uint crc = Crc32(payload.AsSpan(HeaderBytes, PixelBytes));
        WriteHeader(payload, ImageMagic, PixelBytes, crc, frameCount: 1, frameDelayMs: 0);
        return new AnimationImagePayload(payload, FrameCount: 1, DelayMs: 0);
    }

    public byte[] CreatePreviewPng(byte[] sourceBytes, AnimationImageFraming? framing = null)
    {
        AnimationImagePreview preview = CreatePreview(sourceBytes, framing);
        return preview.Frames[0];
    }

    public AnimationImagePreview CreatePreview(byte[] sourceBytes, AnimationImageFraming? framing = null)
    {
        var animatedPreview = TryCreateAnimatedPreview(sourceBytes, framing);
        if (animatedPreview is not null)
            return animatedPreview;

        using var source = SKBitmap.Decode(sourceBytes);
        if (source is null || source.Width <= 0 || source.Height <= 0)
            throw new InvalidOperationException("Formato de imagem nao suportado.");

        return new AnimationImagePreview([EncodePreviewFrame(source, framing)], DefaultFrameDelayMs);
    }

    private static AnimationImagePayload? TryCreateAnimatedUploadPayload(byte[] sourceBytes, AnimationImageFraming? framing)
    {
        using var stream = new MemoryStream(sourceBytes, writable: false);
        using var codec = SKCodec.Create(stream);
        if (codec is null || codec.FrameCount <= 1)
            return null;

        int frameCount = Math.Min(codec.FrameCount, MaxFrames);
        int[] frameIndexes = PickFrameIndexes(codec.FrameCount, frameCount);
        int frameDelayMs = GetAverageFrameDelayMs(codec.FrameInfo, frameIndexes);
        int dataBytes = PixelBytes * frameIndexes.Length;
        byte[] payload = new byte[HeaderBytes + dataBytes];
        var decodeInfo = new SKImageInfo(codec.Info.Width, codec.Info.Height, SKColorType.Rgba8888, SKAlphaType.Premul);

        int offset = HeaderBytes;
        foreach (int frameIndex in frameIndexes)
        {
            using var frame = new SKBitmap(decodeInfo);
            using var pixmap = frame.PeekPixels();
            var result = codec.GetPixels(decodeInfo, pixmap.GetPixels(), new SKCodecOptions(frameIndex, -1));
            if (result != SKCodecResult.Success && result != SKCodecResult.IncompleteInput)
                throw new InvalidOperationException($"Nao foi possivel decodificar o frame {frameIndex + 1} do GIF.");

            using var resized = ResizeToDisplay(frame, framing);
            WriteBitmapPixels(resized, payload.AsSpan(offset, PixelBytes));
            offset += PixelBytes;
        }

        uint crc = Crc32(payload.AsSpan(HeaderBytes, dataBytes));
        WriteHeader(payload, AnimationMagic, dataBytes, crc, frameIndexes.Length, frameDelayMs);
        return new AnimationImagePayload(payload, frameIndexes.Length, frameDelayMs);
    }

    private static AnimationImagePreview? TryCreateAnimatedPreview(byte[] sourceBytes, AnimationImageFraming? framing)
    {
        using var stream = new MemoryStream(sourceBytes, writable: false);
        using var codec = SKCodec.Create(stream);
        if (codec is null || codec.FrameCount <= 1)
            return null;

        int frameCount = Math.Min(codec.FrameCount, MaxFrames);
        int[] frameIndexes = PickFrameIndexes(codec.FrameCount, frameCount);
        int frameDelayMs = GetAverageFrameDelayMs(codec.FrameInfo, frameIndexes);
        var decodeInfo = new SKImageInfo(codec.Info.Width, codec.Info.Height, SKColorType.Rgba8888, SKAlphaType.Premul);
        var frames = new byte[frameIndexes.Length][];

        for (int i = 0; i < frameIndexes.Length; i++)
        {
            using var frame = new SKBitmap(decodeInfo);
            using var pixmap = frame.PeekPixels();
            var result = codec.GetPixels(decodeInfo, pixmap.GetPixels(), new SKCodecOptions(frameIndexes[i], -1));
            if (result != SKCodecResult.Success && result != SKCodecResult.IncompleteInput)
                throw new InvalidOperationException($"Nao foi possivel decodificar o frame {frameIndexes[i] + 1} do GIF.");

            frames[i] = EncodePreviewFrame(frame, framing);
        }

        return new AnimationImagePreview(frames, frameDelayMs);
    }

    private static byte[] EncodePreviewFrame(SKBitmap source, AnimationImageFraming? framing)
    {
        using var resized = ResizeToDisplay(source, framing);
        using var image = SKImage.FromBitmap(resized);
        using var data = image.Encode(SKEncodedImageFormat.Png, 90);
        return data.ToArray();
    }

    private static SKBitmap ResizeToDisplay(SKBitmap source, AnimationImageFraming? framing)
    {
        var imageInfo = new SKImageInfo(Width, Height, SKColorType.Rgba8888, SKAlphaType.Premul);
        var resized = new SKBitmap(imageInfo);
        using var canvas = new SKCanvas(resized);
        using var paint = new SKPaint
        {
            FilterQuality = SKFilterQuality.High,
            IsAntialias = true
        };

        var sourceRect = GetSourceRect(source.Width, source.Height, framing);

        canvas.Clear(SKColors.Black);
        canvas.DrawBitmap(source, sourceRect, new SKRect(0, 0, Width, Height), paint);
        canvas.Flush();
        return resized;
    }

    private static SKRect GetSourceRect(int sourceWidth, int sourceHeight, AnimationImageFraming? framing)
    {
        double zoom = Math.Clamp(framing?.Zoom ?? 1.0, 1.0, 4.0);
        double offsetX = Math.Clamp(framing?.OffsetX ?? 0.0, -1.0, 1.0);
        double offsetY = Math.Clamp(framing?.OffsetY ?? 0.0, -1.0, 1.0);
        double side = Math.Min(sourceWidth, sourceHeight) / zoom;
        double maxCenterOffsetX = Math.Max(0.0, (sourceWidth - side) / 2.0);
        double maxCenterOffsetY = Math.Max(0.0, (sourceHeight - side) / 2.0);
        double centerX = (sourceWidth / 2.0) + (offsetX * maxCenterOffsetX);
        double centerY = (sourceHeight / 2.0) + (offsetY * maxCenterOffsetY);
        double left = Math.Clamp(centerX - side / 2.0, 0.0, sourceWidth - side);
        double top = Math.Clamp(centerY - side / 2.0, 0.0, sourceHeight - side);

        return new SKRect(
            (float)left,
            (float)top,
            (float)(left + side),
            (float)(top + side));
    }

    private static void WriteBitmapPixels(SKBitmap bitmap, Span<byte> target)
    {
        int offset = 0;
        for (int y = 0; y < Height; y++)
        {
            for (int x = 0; x < Width; x++)
            {
                SKColor pixel = bitmap.GetPixel(x, y);
                ushort rgb565 = ToRgb565(pixel);
                BinaryPrimitives.WriteUInt16LittleEndian(target.Slice(offset, 2), rgb565);
                offset += 2;
            }
        }
    }

    private static ushort ToRgb565(SKColor color)
    {
        return (ushort)(((color.Red & 0xF8) << 8) |
                        ((color.Green & 0xFC) << 3) |
                        (color.Blue >> 3));
    }

    private static void WriteHeader(byte[] payload, uint magic, int dataBytes, uint crc, int frameCount, int frameDelayMs)
    {
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(0, 4), magic);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(4, 2), Version);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(6, 2), FormatRgb565LittleEndian);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(8, 2), Width);
        BinaryPrimitives.WriteUInt16LittleEndian(payload.AsSpan(10, 2), Height);
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(12, 4), (uint)dataBytes);
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(16, 4), crc);
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(20, 4), (uint)frameCount);
        BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(24, 4), (uint)frameDelayMs);
    }

    private static int[] PickFrameIndexes(int sourceFrameCount, int targetFrameCount)
    {
        if (targetFrameCount <= 1)
            return [0];

        var indexes = new List<int>(targetFrameCount);
        for (int i = 0; i < targetFrameCount; i++)
        {
            int index = (int)Math.Round(i * (sourceFrameCount - 1) / (double)(targetFrameCount - 1));
            if (indexes.Count == 0 || indexes[^1] != index)
                indexes.Add(index);
        }

        return indexes.ToArray();
    }

    private static int GetAverageFrameDelayMs(IReadOnlyList<SKCodecFrameInfo> frameInfo, IReadOnlyList<int> frameIndexes)
    {
        if (frameIndexes.Count == 0)
            return DefaultFrameDelayMs;

        double totalDurationMs = 0.0;
        foreach (int frameIndex in frameIndexes)
        {
            int duration = frameIndex >= 0 && frameIndex < frameInfo.Count
                ? frameInfo[frameIndex].Duration
                : DefaultFrameDelayMs;
            totalDurationMs += duration <= 0 ? DefaultFrameDelayMs : duration;
        }

        int average = (int)Math.Round(totalDurationMs / frameIndexes.Count);
        return Math.Clamp(average, MinFrameDelayMs, MaxFrameDelayMs);
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
