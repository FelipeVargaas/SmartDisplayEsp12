using SmartDisplayPcAgent.Models;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.Services;

public sealed class DeviceLocationSearchService : IDisposable
{
    private readonly HttpClient _httpClient = new()
    {
        Timeout = TimeSpan.FromSeconds(8),
    };

    public async Task<IReadOnlyList<DeviceLocationOption>> SearchAsync(
        string query,
        string language,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(query))
            return Array.Empty<DeviceLocationOption>();

        string url =
            "https://geocoding-api.open-meteo.com/v1/search" +
            $"?name={Uri.EscapeDataString(query.Trim())}" +
            "&count=6" +
            $"&language={Uri.EscapeDataString(language)}" +
            "&format=json";

        using var response = await _httpClient.GetAsync(url, cancellationToken);
        response.EnsureSuccessStatusCode();

        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        using var document = await JsonDocument.ParseAsync(stream, cancellationToken: cancellationToken);

        if (!document.RootElement.TryGetProperty("results", out var results) ||
            results.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<DeviceLocationOption>();
        }

        return results
            .EnumerateArray()
            .Select(TryCreateOption)
            .Where(option => option is not null)
            .Select(option => option!)
            .ToArray();
    }

    public void Dispose()
    {
        _httpClient.Dispose();
    }

    private static DeviceLocationOption? TryCreateOption(JsonElement element)
    {
        if (!TryGetDouble(element, "latitude", out double latitude) ||
            !TryGetDouble(element, "longitude", out double longitude))
        {
            return null;
        }

        string name = GetString(element, "name", string.Empty);
        if (string.IsNullOrWhiteSpace(name))
            return null;

        string admin = GetString(element, "admin1", string.Empty);
        string country = GetString(element, "country", string.Empty);
        string countryCode = GetString(element, "country_code", string.Empty);
        string timezone = GetString(element, "timezone", "auto");

        string label = string.Join(", ", new[] { name, admin, country }.Where(v => !string.IsNullOrWhiteSpace(v)));

        return new DeviceLocationOption(
            label,
            latitude,
            longitude,
            timezone,
            countryCode.ToUpperInvariant());
    }

    private static string GetString(JsonElement element, string propertyName, string fallback)
    {
        return element.TryGetProperty(propertyName, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? fallback
            : fallback;
    }

    private static bool TryGetDouble(JsonElement element, string propertyName, out double value)
    {
        value = 0;

        if (!element.TryGetProperty(propertyName, out var property))
            return false;

        if (property.ValueKind == JsonValueKind.Number)
            return property.TryGetDouble(out value);

        if (property.ValueKind == JsonValueKind.String)
        {
            return double.TryParse(
                property.GetString(),
                NumberStyles.Float,
                CultureInfo.InvariantCulture,
                out value);
        }

        return false;
    }
}
