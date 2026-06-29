using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.Clients;

public sealed class DeviceControlClient : IDisposable
{
    private readonly HttpClient _httpClient = new()
    {
        Timeout = TimeSpan.FromMilliseconds(2500)
    };

    private readonly HttpClient _uploadHttpClient = new()
    {
        Timeout = TimeSpan.FromSeconds(30)
    };

    public string LastError { get; private set; } = string.Empty;
    public bool LastRequestSkipped { get; private set; }

    public async Task<DeviceStatusSnapshot?> GetStatusAsync(
        string displayAddress,
        CancellationToken cancellationToken,
        bool waitForSlot = false)
    {
        LastRequestSkipped = false;

        if (waitForSlot)
        {
            return await DisplayRequestCoordinator.RunAsync(
                ct => GetStatusCoreAsync(displayAddress, ct),
                TimeSpan.FromMilliseconds(700),
                cancellationToken);
        }

        var result = await DisplayRequestCoordinator.TryRunResultAsync(
            ct => GetStatusCoreAsync(displayAddress, ct),
            cancellationToken);

        LastRequestSkipped = result.Skipped;
        return result.Value;
    }

    private async Task<DeviceStatusSnapshot?> GetStatusCoreAsync(
        string displayAddress,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(displayAddress))
            return null;

        string baseUrl = NormalizeBaseUrl(displayAddress);

        try
        {
            LastError = string.Empty;

            // Cache buster ajuda quando algum proxy/stack resolve reaproveitar resposta antiga.
            using var response = await _httpClient.GetAsync($"{baseUrl}/status?t={Environment.TickCount64}", cancellationToken);

            string responseBody = await response.Content.ReadAsStringAsync(cancellationToken);

            if (!response.IsSuccessStatusCode)
            {
                LastError = $"HTTP {(int)response.StatusCode}: {responseBody}";
                return null;
            }

            using var document = JsonDocument.Parse(responseBody);

            var root = document.RootElement;

            return new DeviceStatusSnapshot(
                Name: GetString(root, "name", "TinyDash"),
                Mode: GetString(root, "mode", "--"),
                Ip: GetString(root, "ip", "--"),
                Ssid: GetString(root, "ssid", "--"),
                Rssi: GetNullableInt(root, "rssi"),
                Theme: GetString(root, "theme", "pc_monitor"),
                Cpu: GetDouble(root, "cpu"),
                Ram: GetDouble(root, "ram"),
                Gpu: GetDouble(root, "gpu"),
                Disk: GetDouble(root, "disk"),
                DiskLabel: GetString(root, "diskLabel", "---"),
                PcOnline: GetBool(root, "pcOnline"),
                LastPcMetricsAgeMs: GetNullableLong(root, "lastPcMetricsAgeMs"),
                UptimeMs: GetNullableLong(root, "uptimeMs"),
                ResetReason: GetString(root, "resetReason", "--"),
                ResetInfo: GetString(root, "resetInfo", "--"),
                RestartIntent: GetString(root, "restartIntent", "--"),
                LastCheckpoint: GetString(root, "lastCheckpoint", "--"),
                Temperature: GetNullableDouble(root, "temperature"),
                Weather: GetString(root, "weather", "--"),
                WeatherStatus: GetString(root, "weatherStatus", "--"),
                Heap: GetNullableLong(root, "heap"),
                HeapFragmentation: GetNullableInt(root, "heapFragmentation"),
                MaxFreeBlockSize: GetNullableLong(root, "maxFreeBlockSize"),
                FlashSize: GetNullableLong(root, "flashSize"),
                AnimationImage: GetBool(root, "animationImage"),
                AnimationImageMaxBytes: GetNullableLong(root, "animationImageMaxBytes"),
                AnimationImageStorageBytes: GetNullableLong(root, "animationImageStorageBytes"),
                LowHeap: GetBool(root, "lowHeap"));
        }
        catch (TaskCanceledException)
        {
            LastError = "Status timeout after 2.5s";
            return null;
        }
        catch (Exception ex)
        {
            LastError = $"{ex.GetType().Name}: {ex.Message}";
            return null;
        }
    }


    public async Task<bool> UploadAnimationImageAsync(
        string displayAddress,
        byte[] payload,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(displayAddress) || payload.Length == 0)
            return false;

        string baseUrl = NormalizeBaseUrl(displayAddress);

        return await DisplayRequestCoordinator.RunBoolAsync(
            ct => UploadAnimationImageCoreAsync(baseUrl, payload, ct),
            TimeSpan.FromSeconds(5),
            cancellationToken);
    }

    private async Task<bool> UploadAnimationImageCoreAsync(
        string baseUrl,
        byte[] payload,
        CancellationToken cancellationToken)
    {
        try
        {
            LastError = string.Empty;

            using var content = new MultipartFormDataContent();
            using var imageContent = new ByteArrayContent(payload);
            imageContent.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("application/octet-stream");
            content.Add(imageContent, "image", "tinydash-animation.tmi");

            using var response = await _uploadHttpClient.PostAsync($"{baseUrl}/animation/image", content, cancellationToken);
            string body = await response.Content.ReadAsStringAsync(cancellationToken);

            if (response.IsSuccessStatusCode)
                return true;

            LastError = $"POST /animation/image HTTP {(int)response.StatusCode}: {body}";
            return false;
        }
        catch (TaskCanceledException)
        {
            LastError = "Animation image upload timeout after 30s";
            return false;
        }
        catch (Exception ex)
        {
            LastError = $"{ex.GetType().Name}: {ex.Message}";
            return false;
        }
    }


    public async Task<bool> SetThemeAsync(
        string displayAddress,
        string themeKey,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(displayAddress) || string.IsNullOrWhiteSpace(themeKey))
            return false;

        string baseUrl = NormalizeBaseUrl(displayAddress);

        return await DisplayRequestCoordinator.RunBoolAsync(
            ct => SetThemeCoreAsync(baseUrl, themeKey, ct),
            TimeSpan.FromSeconds(2),
            cancellationToken);
    }

    private async Task<bool> SetThemeCoreAsync(
        string baseUrl,
        string themeKey,
        CancellationToken cancellationToken)
    {
        try
        {
            LastError = string.Empty;

            string json = JsonSerializer.Serialize(new { theme = themeKey });
            using var content = new StringContent(json, Encoding.UTF8, "application/json");
            using var response = await _httpClient.PostAsync($"{baseUrl}/theme", content, cancellationToken);

            if (response.IsSuccessStatusCode)
                return true;

            string body = await response.Content.ReadAsStringAsync(cancellationToken);
            LastError = $"POST /theme HTTP {(int)response.StatusCode}: {body}";
            return false;
        }
        catch (Exception ex)
        {
            LastError = $"{ex.GetType().Name}: {ex.Message}";
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

    private static string GetString(JsonElement root, string propertyName, string fallback)
    {
        if (!root.TryGetProperty(propertyName, out var property))
            return fallback;

        if (property.ValueKind == JsonValueKind.String)
            return property.GetString() ?? fallback;

        return property.ToString();
    }

    private static double GetDouble(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var property))
            return 0;

        return property.ValueKind switch
        {
            JsonValueKind.Number when property.TryGetDouble(out double value) => value,
            JsonValueKind.String when double.TryParse(property.GetString(), NumberStyles.Any, CultureInfo.InvariantCulture, out double value) => value,
            _ => 0
        };
    }

    private static double? GetNullableDouble(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var property) || property.ValueKind == JsonValueKind.Null)
            return null;

        return property.ValueKind switch
        {
            JsonValueKind.Number when property.TryGetDouble(out double value) => value,
            JsonValueKind.String when double.TryParse(property.GetString(), NumberStyles.Any, CultureInfo.InvariantCulture, out double value) => value,
            _ => null
        };
    }

    private static int? GetNullableInt(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var property) || property.ValueKind == JsonValueKind.Null)
            return null;

        if (property.ValueKind == JsonValueKind.Number && property.TryGetInt32(out int value))
            return value;

        if (property.ValueKind == JsonValueKind.String && int.TryParse(property.GetString(), out value))
            return value;

        return null;
    }

    private static long? GetNullableLong(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var property) || property.ValueKind == JsonValueKind.Null)
            return null;

        if (property.ValueKind == JsonValueKind.Number && property.TryGetInt64(out long value))
            return value;

        if (property.ValueKind == JsonValueKind.String && long.TryParse(property.GetString(), out value))
            return value;

        return null;
    }

    private static bool GetBool(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var property))
            return false;

        return property.ValueKind switch
        {
            JsonValueKind.True => true,
            JsonValueKind.False => false,
            JsonValueKind.Number when property.TryGetInt32(out int value) => value != 0,
            JsonValueKind.String when bool.TryParse(property.GetString(), out bool value) => value,
            _ => false
        };
    }

    public void Dispose()
    {
        _httpClient.Dispose();
        _uploadHttpClient.Dispose();
    }
}
