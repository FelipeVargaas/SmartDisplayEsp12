using SmartDisplayPcAgent.Models;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace SmartDisplayPcAgent.Services;

public sealed class GamerTelemetryService
{
    private const string RtssSharedMemoryName = "RTSSSharedMemoryV2";
    private const int HeaderMinimumSize = 20;
    private const int AppNameOffset = 4;
    private const int AppNameLength = 260;
    private const int AppTime0Offset = 268;
    private const int AppTime1Offset = 272;
    private const int AppFramesOffset = 276;
    private const int AppFrameTimeOffset = 280;
    private const int AppStatFramerateAvgOffset = 308;

    private static readonly string[] IgnoredProcesses =
    [
        "smartdisplaypcagent.exe",
        "tinydashagent.exe",
        "tinydash.exe",
        "dwm.exe",
        "explorer.exe",
        "shellexperiencehost.exe",
        "searchhost.exe",
        "startmenuexperiencehost.exe",
        "applicationframehost.exe",
        "textinputhost.exe",
        "widgets.exe",
        "systemsettings.exe",
        "lockapp.exe",
        "searchapp.exe",
        "taskhostw.exe",
        "runtimebroker.exe",
        "winlogon.exe",
        "audiodg.exe",
        "ms-teams.exe",
        "teams.exe",
        "msteams.exe",
        "whatsapp.exe",
        "chrome.exe",
        "msedge.exe",
        "firefox.exe",
        "discord.exe",
        "telegram.exe",
        "slack.exe",
        "steam.exe",
        "steamwebhelper.exe",
        "epicgameslauncher.exe",
        "battle.net.exe",
        "obs64.exe",
        "obs32.exe",
        "afterburner.exe",
        "rtss.exe",
        "rtsshooksloader64.exe",
        "rtsshooksloader.exe"
    ];

    public GamerTelemetrySnapshot GetSnapshot()
    {
        try
        {
            using var mappedFile = MemoryMappedFile.OpenExisting(
                RtssSharedMemoryName,
                MemoryMappedFileRights.Read);

            using var accessor = mappedFile.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);

            byte[] header = new byte[Math.Min(512, accessor.Capacity > 512 ? 512 : (int)accessor.Capacity)];
            accessor.ReadArray(0, header, 0, header.Length);

            if (header.Length < HeaderMinimumSize)
                return new GamerTelemetrySnapshot(Status: "RTSS OFF");

            uint appEntrySize = ReadUInt32(header, 8);
            uint appArrayOffset = ReadUInt32(header, 12);
            uint appArraySize = ReadUInt32(header, 16);

            if (appEntrySize < 320 || appArrayOffset == 0 || appArraySize == 0)
                return new GamerTelemetrySnapshot(Status: "RTSS WAITING");

            var candidates = new List<RtssAppCandidate>();

            for (uint i = 0; i < appArraySize; i++)
            {
                long entryOffset = appArrayOffset + (i * appEntrySize);

                if (entryOffset < 0 || entryOffset + appEntrySize > accessor.Capacity)
                    break;

                int entrySize = (int)Math.Min(appEntrySize, 1024);
                byte[] entry = new byte[entrySize];
                accessor.ReadArray(entryOffset, entry, 0, entrySize);

                var candidate = ParseAppEntry(entry);
                if (candidate is not null)
                    candidates.Add(candidate);
            }

            return SelectBestCandidate(candidates, GetForegroundProcessName());
        }
        catch (FileNotFoundException)
        {
            return new GamerTelemetrySnapshot(Status: "RTSS OFF");
        }
        catch
        {
            return new GamerTelemetrySnapshot(Status: "RTSS ERROR");
        }
    }

    private static GamerTelemetrySnapshot SelectBestCandidate(
        IReadOnlyList<RtssAppCandidate> candidates,
        string? foregroundProcessName)
    {
        if (candidates.Count == 0)
            return new GamerTelemetrySnapshot(Status: "WAITING GAME");

        string? foreground = NormalizeProcessName(foregroundProcessName ?? string.Empty);

        if (!string.IsNullOrWhiteSpace(foreground) && !IsIgnoredProcess(foreground))
        {
            var foregroundCandidate = candidates
                .Where(c => c.HasValidData)
                .FirstOrDefault(c => string.Equals(c.Game, foreground, StringComparison.OrdinalIgnoreCase));

            if (foregroundCandidate is not null)
                return foregroundCandidate.ToSnapshot();
        }

        var validGameCandidates = candidates
            .Where(c => c.HasValidData)
            .Where(c => !IsIgnoredProcess(c.Game))
            .Where(c => !LooksLikeBackgroundUi(c))
            .OrderByDescending(c => c.Fps ?? 0)
            .ThenBy(c => c.Game, StringComparer.OrdinalIgnoreCase)
            .ToList();

        if (validGameCandidates.Count > 0)
            return validGameCandidates[0].ToSnapshot();

        var fallback = candidates
            .Where(c => !IsIgnoredProcess(c.Game))
            .OrderBy(c => c.Game, StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault();

        if (fallback is not null)
        {
            return new GamerTelemetrySnapshot(
                Game: fallback.Game,
                Fps: null,
                Frametime: null,
                Source: null,
                Status: "WAITING GAME");
        }

        return new GamerTelemetrySnapshot(Status: "WAITING GAME");
    }

    private static RtssAppCandidate? ParseAppEntry(byte[] entry)
    {
        if (entry.Length < AppNameOffset + AppNameLength)
            return null;

        uint processId = ReadUInt32(entry, 0);
        if (processId == 0)
            return null;

        string rawName = ReadAsciiString(entry, AppNameOffset, AppNameLength);
        string game = NormalizeProcessName(rawName);

        if (string.IsNullOrWhiteSpace(game))
            return null;

        double? frametime = null;
        if (entry.Length >= AppFrameTimeOffset + 4)
        {
            uint rawFrameTime = ReadUInt32(entry, AppFrameTimeOffset);
            frametime = NormalizeFrameTime(rawFrameTime);
        }

        int? fps = null;
        if (frametime.HasValue && frametime.Value > 0.1)
        {
            double calculatedFps = 1000.0 / frametime.Value;
            if (calculatedFps is > 0 and < 1000)
                fps = (int)Math.Round(calculatedFps);
        }

        if (!fps.HasValue && entry.Length >= AppStatFramerateAvgOffset + 4)
            fps = NormalizeFramerate(ReadUInt32(entry, AppStatFramerateAvgOffset));

        if (!fps.HasValue && entry.Length >= AppFramesOffset + 4 && entry.Length >= AppTime1Offset + 4)
        {
            fps = CalculateFramerateFromWindow(
                ReadUInt32(entry, AppFramesOffset),
                ReadUInt32(entry, AppTime0Offset),
                ReadUInt32(entry, AppTime1Offset));
        }

        return new RtssAppCandidate(game, fps, frametime);
    }

    private static bool LooksLikeBackgroundUi(RtssAppCandidate candidate)
    {
        // Muitos apps de desktop aparecem no RTSS com FPS baixo próprio.
        // Se não estiverem em foco e tiverem FPS muito baixo, tratamos como ruído.
        if (!candidate.Fps.HasValue)
            return false;

        return candidate.Fps.Value is > 0 and < 10;
    }

    private static double? NormalizeFrameTime(uint rawFrameTime)
    {
        if (rawFrameTime == 0)
            return null;

        // RTSS costuma armazenar frametime em microssegundos.
        // Alguns builds/fontes podem expor milissegundos; por isso há fallback conservador.
        double value = rawFrameTime >= 1000
            ? rawFrameTime / 1000.0
            : rawFrameTime;

        if (value <= 0 || value > 1000)
            return null;

        return Math.Round(value, 1);
    }

    private static int? NormalizeFramerate(uint rawFramerate)
    {
        if (rawFramerate == 0)
            return null;

        double value = rawFramerate;

        // RTSS normalmente usa FPS x10 em estatísticas, ex.: 1440 = 144.0 FPS.
        if (value > 1000 && value < 10000)
            value /= 10.0;

        if (value <= 0 || value > 1000)
            return null;

        return (int)Math.Round(value);
    }

    private static int? CalculateFramerateFromWindow(uint frames, uint time0, uint time1)
    {
        if (frames == 0 || time1 <= time0)
            return null;

        double elapsedMs = time1 - time0;
        double fps = frames * 1000.0 / elapsedMs;

        if (fps <= 0 || fps > 1000)
            return null;

        return (int)Math.Round(fps);
    }

    private static string NormalizeProcessName(string rawName)
    {
        string value = rawName.Trim();

        if (string.IsNullOrWhiteSpace(value))
            return string.Empty;

        try
        {
            value = Path.GetFileName(value);
        }
        catch
        {
            // Mantém o valor bruto se Path.GetFileName não aceitar a string.
        }

        if (value.Length > 48)
            value = value[..48];

        return value;
    }

    private static bool IsIgnoredProcess(string processName)
    {
        if (string.IsNullOrWhiteSpace(processName))
            return true;

        return IgnoredProcesses.Contains(processName.Trim(), StringComparer.OrdinalIgnoreCase);
    }

    private static string? GetForegroundProcessName()
    {
        try
        {
            IntPtr windowHandle = GetForegroundWindow();
            if (windowHandle == IntPtr.Zero)
                return null;

            _ = GetWindowThreadProcessId(windowHandle, out uint processId);
            if (processId == 0)
                return null;

            using var process = Process.GetProcessById((int)processId);
            string name = process.ProcessName;

            return name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)
                ? name
                : $"{name}.exe";
        }
        catch
        {
            return null;
        }
    }

    private static string ReadAsciiString(byte[] buffer, int offset, int length)
    {
        if (offset < 0 || length <= 0 || offset >= buffer.Length)
            return string.Empty;

        int maxLength = Math.Min(length, buffer.Length - offset);
        int actualLength = 0;

        while (actualLength < maxLength && buffer[offset + actualLength] != 0)
            actualLength++;

        return Encoding.ASCII.GetString(buffer, offset, actualLength).Trim();
    }

    private static uint ReadUInt32(byte[] buffer, int offset)
    {
        if (offset < 0 || offset + 4 > buffer.Length)
            return 0;

        return BitConverter.ToUInt32(buffer, offset);
    }

    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    private sealed record RtssAppCandidate(
        string Game,
        int? Fps,
        double? Frametime)
    {
        public bool HasValidData => Fps.HasValue || Frametime.HasValue;

        public GamerTelemetrySnapshot ToSnapshot()
        {
            return new GamerTelemetrySnapshot(
                Game: Game,
                Fps: Fps,
                Frametime: Frametime,
                Source: HasValidData ? "RTSS" : null,
                Status: HasValidData ? "RTSS" : "WAITING GAME");
        }
    }
}
