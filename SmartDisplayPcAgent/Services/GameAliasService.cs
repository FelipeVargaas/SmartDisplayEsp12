using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace SmartDisplayPcAgent.Services;

public sealed class GameAliasService
{
    private readonly string _filePath;
    private Dictionary<string, string> _aliases = new(StringComparer.OrdinalIgnoreCase);

    public GameAliasService()
    {
        string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        string directory = Path.Combine(appData, "TinyDash Agent");
        Directory.CreateDirectory(directory);
        _filePath = Path.Combine(directory, "game-aliases.json");
        Load();
    }

    public string Resolve(string processName)
    {
        string key = NormalizeProcessName(processName);
        if (key.Length == 0) return string.Empty;

        return _aliases.TryGetValue(key, out string? alias) && !string.IsNullOrWhiteSpace(alias)
            ? alias
            : key;
    }

    public string GetAlias(string processName)
    {
        string key = NormalizeProcessName(processName);
        if (key.Length == 0) return string.Empty;

        return _aliases.TryGetValue(key, out string? alias) ? alias : string.Empty;
    }

    public void SaveAlias(string processName, string displayName)
    {
        string key = NormalizeProcessName(processName);
        string value = displayName.Trim();
        if (key.Length == 0 || value.Length == 0) return;

        _aliases[key] = value.Length <= 48 ? value : value[..48];
        Save();
    }

    public void DeleteAlias(string processName)
    {
        string key = NormalizeProcessName(processName);
        if (key.Length == 0) return;

        if (_aliases.Remove(key))
            Save();
    }

    private void Load()
    {
        if (!File.Exists(_filePath))
            return;

        try
        {
            string json = File.ReadAllText(_filePath);
            var model = JsonSerializer.Deserialize<GameAliasesFile>(json);
            _aliases = model?.Aliases is null
                ? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                : new Dictionary<string, string>(model.Aliases, StringComparer.OrdinalIgnoreCase);
        }
        catch
        {
            _aliases = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        }
    }

    private void Save()
    {
        var model = new GameAliasesFile(new SortedDictionary<string, string>(_aliases, StringComparer.OrdinalIgnoreCase));
        var options = new JsonSerializerOptions { WriteIndented = true };
        File.WriteAllText(_filePath, JsonSerializer.Serialize(model, options));
    }

    private static string NormalizeProcessName(string value)
    {
        value = value.Trim();
        if (value.Length == 0 || value == "--") return string.Empty;

        try
        {
            value = Path.GetFileName(value);
        }
        catch
        {
        }

        return value.Length <= 48 ? value : value[..48];
    }

    private sealed record GameAliasesFile(IDictionary<string, string> Aliases);
}
