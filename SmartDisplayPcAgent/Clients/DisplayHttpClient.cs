using System;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using SmartDisplayPcAgent.Models;

namespace SmartDisplayPcAgent.Clients;

public sealed class DisplayHttpClient : IDisposable
{
    private readonly HttpClient _httpClient = new()
    {
        Timeout = TimeSpan.FromMilliseconds(900)
    };

    public async Task<bool> SendMetricsAsync(
        string displayAddress,
        PcMetricsSnapshot snapshot,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(displayAddress))
            return false;

        string baseUrl = NormalizeBaseUrl(displayAddress);

        var payload = new
        {
            cpu = ToPercentInt(snapshot.CpuUsage),
            ram = ToPercentInt(snapshot.RamUsage),
            gpu = ToPercentInt(snapshot.GpuUsage),
            disk = ToPercentInt(snapshot.DiskUsage),
            diskLabel = NormalizeDiskLabel(snapshot.DiskLabel)
        };

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

    private static string NormalizeDiskLabel(string label)
    {
        if (string.IsNullOrWhiteSpace(label))
            return "---";

        string value = label.Trim().ToUpperInvariant();

        if (value.Length > 3)
            value = value[..3];

        return value;
    }

    public void Dispose()
    {
        _httpClient.Dispose();
    }
}