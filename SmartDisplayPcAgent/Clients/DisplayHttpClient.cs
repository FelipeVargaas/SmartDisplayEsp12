using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.Clients;

public sealed class DisplayHttpClient : IDisposable
{
    private readonly HttpClient _httpClient = new()
    {
        Timeout = TimeSpan.FromMilliseconds(1800)
    };

    public bool LastSendSkipped { get; private set; }

    public async Task<bool> SendMetricsAsync(
        string displayAddress,
        PcMetricsSnapshot snapshot,
        CancellationToken cancellationToken,
        bool includeDisk = true,
        bool includeGamerTelemetry = false,
        bool waitForSlot = false)
    {
        LastSendSkipped = false;

        if (waitForSlot)
        {
            return await DisplayRequestCoordinator.RunBoolAsync(
                ct => SendMetricsCoreAsync(displayAddress, snapshot, ct, includeDisk, includeGamerTelemetry),
                TimeSpan.FromMilliseconds(500),
                cancellationToken);
        }

        bool? result = await DisplayRequestCoordinator.TryRunBoolAsync(
            ct => SendMetricsCoreAsync(displayAddress, snapshot, ct, includeDisk, includeGamerTelemetry),
            cancellationToken);

        if (!result.HasValue)
        {
            LastSendSkipped = true;
            return false;
        }

        return result.Value;
    }

    private async Task<bool> SendMetricsCoreAsync(
        string displayAddress,
        PcMetricsSnapshot snapshot,
        CancellationToken cancellationToken,
        bool includeDisk,
        bool includeGamerTelemetry)
    {
        if (string.IsNullOrWhiteSpace(displayAddress))
            return false;

        string baseUrl = NormalizeBaseUrl(displayAddress);

        var payload = new Dictionary<string, object?>
        {
            ["cpu"] = ToPercentInt(snapshot.CpuUsage),
            ["ram"] = ToPercentInt(snapshot.RamUsage),
            ["gpu"] = ToPercentInt(snapshot.GpuUsage)
        };

        if (includeDisk)
        {
            payload["disk"] = ToPercentInt(snapshot.DiskUsage);
            payload["diskLabel"] = NormalizeDiskLabel(snapshot.DiskLabel);
        }

        if (includeGamerTelemetry)
        {
            int? gpuTemp = ToNullableInt(snapshot.GpuTemperature);
            string game = NormalizeGameName(snapshot.Game);
            double? frametime = NormalizeFrametime(snapshot.Frametime);
            string? source = NormalizeSource(snapshot.Source);

            if (gpuTemp.HasValue) payload["gpuTemp"] = gpuTemp.Value;
            payload["game"] = game;
            payload["fps"] = snapshot.Fps.HasValue ? snapshot.Fps.Value : null;
            payload["frametime"] = frametime.HasValue ? frametime.Value : null;
            payload["source"] = source;
        }

        string json = JsonSerializer.Serialize(payload);

        using var content = new StringContent(json, Encoding.UTF8, "application/json");

        try
        {
            using var response = await _httpClient.PostAsync(
                $"{baseUrl}/metrics",
                content,
                cancellationToken);

            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    private static string NormalizeBaseUrl(string address)
    {
        string value = address.Trim();

        if (!value.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
            !value.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            value = "http://" + value;
        }

        return value.TrimEnd('/');
    }

    private static int ToPercentInt(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value))
            return 0;

        return Math.Clamp((int)Math.Round(value), 0, 100);
    }

    private static int? ToNullableInt(double? value)
    {
        if (!value.HasValue)
            return null;

        if (double.IsNaN(value.Value) || double.IsInfinity(value.Value))
            return null;

        return (int)Math.Round(value.Value);
    }

    private static double? NormalizeFrametime(double? value)
    {
        if (!value.HasValue)
            return null;

        if (double.IsNaN(value.Value) || double.IsInfinity(value.Value) || value.Value < 0)
            return null;

        return Math.Round(value.Value, 1);
    }

    private static string NormalizeDiskLabel(string label)
    {
        if (string.IsNullOrWhiteSpace(label))
            return "---";

        string value = label.Trim().ToUpperInvariant();

        if (value.Length > 3)
            value = value[..3];

        return value;
    }

    private static string NormalizeGameName(string? game)
    {
        if (string.IsNullOrWhiteSpace(game))
            return string.Empty;

        string value = game.Trim();

        return value.Length <= 48
            ? value
            : value[..48];
    }

    private static string? NormalizeSource(string? source)
    {
        if (string.IsNullOrWhiteSpace(source))
            return null;

        string value = source.Trim();

        return value.Length <= 12
            ? value
            : value[..12];
    }

    public void Dispose()
    {
        _httpClient.Dispose();
    }
}
