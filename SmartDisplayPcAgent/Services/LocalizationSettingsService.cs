using System;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace SmartDisplayPcAgent.Services;

public sealed class LocalizationSettingsService
{
    public const string DefaultCultureName = "pt-BR";

    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    private readonly string _settingsPath;

    public LocalizationSettingsService()
    {
        string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        string directory = Path.Combine(appData, "TinyDash Agent");
        _settingsPath = Path.Combine(directory, "settings.json");
    }

    public static void ApplySavedCulture()
    {
        string cultureName = new LocalizationSettingsService().LoadCultureName();
        ApplyCulture(cultureName);
    }

    public static void ApplyCulture(string cultureName)
    {
        string normalizedCultureName = NormalizeCultureName(cultureName);
        CultureInfo culture = CultureInfo.GetCultureInfo(normalizedCultureName);
        CultureInfo.DefaultThreadCurrentCulture = culture;
        CultureInfo.DefaultThreadCurrentUICulture = culture;
        CultureInfo.CurrentCulture = culture;
        CultureInfo.CurrentUICulture = culture;
    }

    public string LoadCultureName()
    {
        try
        {
            if (!File.Exists(_settingsPath))
                return DefaultCultureName;

            string json = File.ReadAllText(_settingsPath);
            var settings = JsonSerializer.Deserialize<SettingsFile>(json);
            return NormalizeCultureName(settings?.CultureName);
        }
        catch
        {
            return DefaultCultureName;
        }
    }

    public void SaveCultureName(string cultureName)
    {
        string normalizedCultureName = NormalizeCultureName(cultureName);
        string? directory = Path.GetDirectoryName(_settingsPath);
        if (!string.IsNullOrWhiteSpace(directory))
            Directory.CreateDirectory(directory);

        var settings = new SettingsFile(normalizedCultureName);
        string json = JsonSerializer.Serialize(settings, JsonOptions);
        File.WriteAllText(_settingsPath, json);
    }

    public static string NormalizeCultureName(string? cultureName)
    {
        return cultureName?.Trim().ToLowerInvariant() switch
        {
            "en" or "en-us" => "en",
            "pt" or "pt-br" => "pt-BR",
            _ => DefaultCultureName
        };
    }

    private sealed record SettingsFile(string CultureName);
}
