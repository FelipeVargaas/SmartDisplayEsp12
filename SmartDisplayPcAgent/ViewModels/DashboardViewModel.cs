using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SmartDisplayPcAgent.Clients;
using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.ViewModels;

public partial class DashboardViewModel : ObservableObject, IDisposable
{
    private readonly PcMetricsService _metricsService = new();
    private readonly DisplayHttpClient _displayHttpClient = new();
    private readonly DeviceControlClient _deviceControlClient = new();
    private readonly HardwareInfoService _hardwareInfoService = new();
    private readonly GamerTelemetryService _gamerTelemetryService = new();
    private readonly CancellationTokenSource _cts = new();

    private DateTime _lastDeviceStatusRefresh = DateTime.MinValue;
    private bool _isDeviceStatusRefreshRunning;
    private string _activeThemeKey = "pc_monitor";

    public DashboardViewModel(AgentConnectionState state)
    {
        State = state;
        LoadHardwareSummary();
        UpdateThemePanel(_activeThemeKey);
        _ = Task.Run(() => StartMetricsLoopAsync(_cts.Token));
    }

    public AgentConnectionState State { get; }

    [ObservableProperty]
    private double cpuUsage;

    [ObservableProperty]
    private double ramUsage;

    [ObservableProperty]
    private double gpuUsage;

    [ObservableProperty]
    private double? gpuTemperature;

    [ObservableProperty]
    private string gpuTemperatureDisplayCompactText = "--";

    [ObservableProperty]
    private string statusText = "Iniciando coleta...";

    [ObservableProperty]
    private string displayStatusText = "Envio para display desativado";

    [ObservableProperty]
    private string displayStatusShortText = "Paused";

    [ObservableProperty]
    private string espHeaderStatusText = "ESP READY";

    [ObservableProperty]
    private string lastPostText = "Disabled";

    [ObservableProperty]
    private double diskUsage;

    [ObservableProperty]
    private string diskLabel = "---";

    [ObservableProperty]
    private string disksText = "Nenhum disco detectado";

    [ObservableProperty]
    private string activeThemeText = "PC Monitor";

    [ObservableProperty]
    private string deviceHeaderText = "No device status yet";

    [ObservableProperty]
    private string themePanelTitle = "PC MONITOR";

    [ObservableProperty]
    private string themePanelBodyText = "Telemetry bridge active";

    [ObservableProperty]
    private string themePanelStatusText = "Waiting for first device status";

    [ObservableProperty]
    private string cpuNameText = "Detecting CPU...";

    [ObservableProperty]
    private string cpuCoresText = "--";

    [ObservableProperty]
    private string cpuThreadsText = "--";

    [ObservableProperty]
    private string cpuClockText = "--";

    [ObservableProperty]
    private string memoryTotalText = "--";

    [ObservableProperty]
    private string memorySpeedText = "--";

    [ObservableProperty]
    private string memoryModulesText = "--";

    [ObservableProperty]
    private string memoryChannelText = "Unknown";

    [ObservableProperty]
    private string gpuNameText = "Detecting GPU...";

    [ObservableProperty]
    private string gpuVramText = "--";

    [ObservableProperty]
    private string gpuClockText = "--";

    [ObservableProperty]
    private string pcSummaryGpuTemperatureText = "--";

    [ObservableProperty]
    private string storageNameText = "Storage";

    [ObservableProperty]
    private string storageTotalText = "--";

    [ObservableProperty]
    private string storageUsedText = "--";

    [ObservableProperty]
    private double storageUsedPercent;

    [ObservableProperty]
    private string gamerGameText = "--";

    [ObservableProperty]
    private string gamerFpsText = "--";

    [ObservableProperty]
    private string gamerFrametimeText = "--";

    [ObservableProperty]
    private string gamerSourceText = "RTSS OFF";

    [ObservableProperty]
    private string gamerStatusText = "RTSS OFF";

    private async Task StartMetricsLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            try
            {
                var snapshot = _metricsService.GetSnapshot();
                var gamerTelemetry = _gamerTelemetryService.GetSnapshot();

                snapshot = snapshot with
                {
                    Game = gamerTelemetry.Game,
                    Fps = gamerTelemetry.Fps,
                    Frametime = gamerTelemetry.Frametime,
                    Source = gamerTelemetry.Source
                };

                string gamerGameText = string.IsNullOrWhiteSpace(gamerTelemetry.Game) ? "--" : gamerTelemetry.Game;
                string gamerFpsText = gamerTelemetry.Fps.HasValue ? gamerTelemetry.Fps.Value.ToString() : "--";
                string gamerFrametimeText = gamerTelemetry.Frametime.HasValue ? $"{gamerTelemetry.Frametime.Value:0.0} ms" : "--";
                string gamerSourceText = string.IsNullOrWhiteSpace(gamerTelemetry.Source) ? "RTSS OFF" : gamerTelemetry.Source;
                string gamerStatusText = gamerTelemetry.Status;

                double cpu = Math.Round(snapshot.CpuUsage);
                double ram = Math.Round(snapshot.RamUsage);
                double gpu = Math.Round(snapshot.GpuUsage);

                double? gpuTemperature = snapshot.GpuTemperature.HasValue
                    ? Math.Round(snapshot.GpuTemperature.Value)
                    : null;

                string gpuTemperatureDisplayCompactText = FormatTemperatureCompact(snapshot.GpuTemperature);

                double disk = Math.Round(snapshot.DiskUsage);
                string diskLabel = snapshot.DiskLabel;
                string disksText = BuildDisksText(snapshot.Disks);

                string displayStatus = DisplayStatusText;
                string displayStatusShort = DisplayStatusShortText;
                string espHeaderStatus = EspHeaderStatusText;
                string lastPostText = LastPostText;

                if (State.SendToDisplayEnabled)
                {
                    string activeThemeKey = NormalizeThemeKey(_activeThemeKey);
                    bool sent = await _displayHttpClient.SendMetricsAsync(
                        State.DisplayIp,
                        snapshot,
                        cancellationToken,
                        includeDisk: ShouldSendDiskMetrics(activeThemeKey),
                        includeGamerTelemetry: activeThemeKey == "gamer");

                    if (_displayHttpClient.LastSendSkipped)
                    {
                        bool hasRecentEspSuccess = DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45));
                        displayStatus = "ESP ocupado, envio pulado";
                        displayStatusShort = hasRecentEspSuccess ? "Online" : "Waiting";
                        espHeaderStatus = hasRecentEspSuccess ? "ESP ONLINE" : "ESP WAIT";
                        lastPostText = "Skipped";
                    }
                    else
                    {

                    displayStatus = sent
                        ? $"Enviado para {State.DisplayIp}"
                        : $"Falha ao enviar para {State.DisplayIp}";

                    displayStatusShort = sent ? "Online" : DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45)) ? "Unstable" : "Offline";
                    espHeaderStatus = sent ? "ESP ONLINE" : DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45)) ? "ESP INSTAVEL" : "ESP OFFLINE";
                    lastPostText = sent ? "OK · 1s ago" : "Failed";
                }
                }
                else
                {
                    displayStatus = "Envio para display desativado";
                    displayStatusShort = "Paused";
                    lastPostText = "Disabled";
                }

                if ((DateTime.Now - _lastDeviceStatusRefresh).TotalMilliseconds >= 30000 && !_isDeviceStatusRefreshRunning)
                {
                    _lastDeviceStatusRefresh = DateTime.Now;
                    _isDeviceStatusRefreshRunning = true;

                    try
                    {
                        var deviceStatus = await _deviceControlClient.GetStatusAsync(State.DisplayIp, cancellationToken);

                        if (deviceStatus is not null)
                        {
                            _activeThemeKey = NormalizeThemeKey(deviceStatus.Theme);
                            string themeText = FormatThemeName(_activeThemeKey);
                            string deviceHeader = $"{themeText} · {deviceStatus.Ip}";
                            espHeaderStatus = "ESP ONLINE";

                            Dispatcher.UIThread.Post(() =>
                            {
                                ActiveThemeText = themeText;
                                DeviceHeaderText = deviceHeader;
                                EspHeaderStatusText = "ESP ONLINE";
                                UpdateThemePanel(_activeThemeKey);
                            });
                        }
                        else
                        {
                            bool hasRecentEspSuccess = DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45));
                            espHeaderStatus = _deviceControlClient.LastRequestSkipped
                                ? hasRecentEspSuccess ? "ESP ONLINE" : "ESP WAIT"
                                : hasRecentEspSuccess ? "ESP INSTAVEL" : "ESP OFFLINE";

                            Dispatcher.UIThread.Post(() =>
                            {
                                DeviceHeaderText = $"No response · {State.DisplayIp}";
                                EspHeaderStatusText = espHeaderStatus;
                            });
                        }
                    }
                    finally
                    {
                        _isDeviceStatusRefreshRunning = false;
                    }
                }

                Dispatcher.UIThread.Post(() =>
                {
                    CpuUsage = cpu;
                    RamUsage = ram;
                    GpuUsage = gpu;
                    GpuTemperature = gpuTemperature;
                    GpuTemperatureDisplayCompactText = gpuTemperatureDisplayCompactText;
                    PcSummaryGpuTemperatureText = gpuTemperatureDisplayCompactText;
                    GamerGameText = gamerGameText;
                    GamerFpsText = gamerFpsText;
                    GamerFrametimeText = gamerFrametimeText;
                    GamerSourceText = gamerSourceText;
                    GamerStatusText = gamerStatusText;
                    StatusText = "Coletando dados locais";
                    DisplayStatusText = displayStatus;
                    DisplayStatusShortText = displayStatusShort;
                    EspHeaderStatusText = espHeaderStatus;
                    LastPostText = lastPostText;

                    State.DisplayStatusText = displayStatus;
                    State.DisplayStatusShortText = displayStatusShort;
                    State.LastPostText = lastPostText;
                    DiskUsage = disk;
                    DiskLabel = diskLabel;
                    DisksText = disksText;
                    UpdateThemePanel(_activeThemeKey);
                });
            }
            catch (Exception ex)
            {
                Dispatcher.UIThread.Post(() =>
                {
                    StatusText = $"Erro na coleta: {ex.Message}";
                });
            }

            try
            {
                await Task.Delay(1000, cancellationToken);
            }
            catch (TaskCanceledException)
            {
                break;
            }
        }
    }

    [RelayCommand]
    private async Task SendTestAsync()
    {
        DisplayStatusText = "Testando envio...";

        var snapshot = new PcMetricsSnapshot(
            CpuUsage,
            RamUsage,
            GpuUsage,
            DiskUsage,
            DiskLabel,
            GpuTemperature: GpuTemperature,
            Game: GamerGameText == "--" ? string.Empty : GamerGameText,
            Fps: int.TryParse(GamerFpsText, out int fps) ? fps : null,
            Frametime: ParseFrametimeText(GamerFrametimeText),
            Source: GamerSourceText == "RTSS" ? "RTSS" : null);

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(2));

        bool sent = await _displayHttpClient.SendMetricsAsync(
            State.DisplayIp,
            snapshot,
            timeoutCts.Token,
            includeDisk: ShouldSendDiskMetrics(_activeThemeKey),
            includeGamerTelemetry: NormalizeThemeKey(_activeThemeKey) == "gamer",
            waitForSlot: true);

        DisplayStatusText = sent
            ? $"Teste enviado para {State.DisplayIp}"
            : $"Falha no teste para {State.DisplayIp}";

        DisplayStatusShortText = sent ? "Online" : "Offline";
        EspHeaderStatusText = sent ? "ESP ONLINE" : "ESP OFFLINE";
        LastPostText = sent ? "OK · now" : "Failed";

        State.DisplayStatusText = DisplayStatusText;
        State.DisplayStatusShortText = DisplayStatusShortText;
        State.LastPostText = LastPostText;
    }

    public void Dispose()
    {
        _cts.Cancel();
        _cts.Dispose();

        _metricsService.Dispose();
        _displayHttpClient.Dispose();
        _deviceControlClient.Dispose();
        _hardwareInfoService.Dispose();
    }

    private void LoadHardwareSummary()
    {
        try
        {
            var hardware = _hardwareInfoService.GetSnapshot();

            CpuNameText = hardware.CpuName;
            CpuCoresText = hardware.CpuCoresText;
            CpuThreadsText = hardware.CpuThreadsText;
            CpuClockText = hardware.CpuClockText;

            MemoryTotalText = hardware.MemoryTotalText;
            MemorySpeedText = hardware.MemorySpeedText;
            MemoryModulesText = hardware.MemoryModulesText;
            MemoryChannelText = hardware.MemoryChannelText;

            GpuNameText = hardware.GpuName;
            GpuVramText = hardware.GpuVramText;
            GpuClockText = hardware.GpuClockText;

            StorageNameText = hardware.StorageName;
            StorageTotalText = hardware.StorageTotalText;
            StorageUsedText = hardware.StorageUsedText;
            StorageUsedPercent = hardware.StorageUsedPercent;
        }
        catch
        {
            // Mantém os placeholders. Hardware info é diagnóstico, não pode derrubar o Agent.
        }
    }

    private void UpdateThemePanel(string themeKey)
    {
        switch (NormalizeThemeKey(themeKey))
        {
            case "gamer":
                ThemePanelTitle = "GAMER HUD";
                ThemePanelBodyText =
                    $"Game          {GamerGameText}{Environment.NewLine}" +
                    $"FPS           {GamerFpsText}{Environment.NewLine}" +
                    $"Frametime     {GamerFrametimeText}{Environment.NewLine}" +
                    $"GPU Temp      {GpuTemperatureDisplayCompactText}{Environment.NewLine}" +
                    $"Source        {GamerSourceText}";
                ThemePanelStatusText = GamerStatusText == "RTSS"
                    ? "RTSS ativo enviando FPS/frametime"
                    : GamerStatusText;
                break;

            case "minimal_clock":
                ThemePanelTitle = "MINIMAL CLOCK";
                ThemePanelBodyText =
                    $"Clock color   Soon{Environment.NewLine}" +
                    $"Accent color  Soon{Environment.NewLine}" +
                    $"Seconds       Soon{Environment.NewLine}" +
                    $"Weather       Soon";
                ThemePanelStatusText = "Configuração visual entra depois do RTSS";
                break;

            case "work_desk":
                ThemePanelTitle = "WORK DESK";
                ThemePanelBodyText =
                    $"Calendar      Not configured{Environment.NewLine}" +
                    $"Email         Not configured{Environment.NewLine}" +
                    $"WhatsApp      Not configured";
                ThemePanelStatusText = "Fontes de notificação ainda não configuradas";
                break;

            case "animation":
                ThemePanelTitle = "ANIMATION";
                ThemePanelBodyText =
                    $"Image/GIF     Soon{Environment.NewLine}" +
                    $"Validation    Soon{Environment.NewLine}" +
                    $"Upload        Soon";
                ThemePanelStatusText = "Conversão e envio serão feitos pelo Agent";
                break;

            default:
                ThemePanelTitle = "PC MONITOR";
                ThemePanelBodyText =
                    $"Telemetry bridge active{Environment.NewLine}" +
                    $"Required      CPU RAM GPU{Environment.NewLine}" +
                    $"Optional      GPU Temp Disk{Environment.NewLine}" +
                    $"Payload       Valid";
                ThemePanelStatusText = "Tema principal usando métricas locais";
                break;
        }
    }

    private static double? ParseFrametimeText(string text)
    {
        if (string.IsNullOrWhiteSpace(text) || text.Trim() == "--")
            return null;

        string value = text
            .Replace("ms", string.Empty, StringComparison.OrdinalIgnoreCase)
            .Trim();

        return double.TryParse(value, out double parsed)
            ? parsed
            : null;
    }

    private static string BuildDisksText(IReadOnlyList<DiskMetricsSnapshot>? disks)
    {
        if (disks is null || disks.Count == 0)
            return "Nenhum disco detectado";

        return string.Join(
            Environment.NewLine,
            disks
                .OrderByDescending(d => d.IsSystemDisk)
                .ThenBy(d => d.Label)
                .Select(d =>
                    $"{d.Label}  {d.DriveLetter}  " +
                    $"Ativ {d.ActiveUsage:0}%  " +
                    $"Usado {d.UsedSpace:0}%  " +
                    $"R {d.ReadMbPerSecond:0.0}  " +
                    $"W {d.WriteMbPerSecond:0.0}"));
    }

    private static string FormatTemperatureCompact(double? temperature)
    {
        return temperature.HasValue
            ? $"{temperature.Value:0}°C"
            : "--";
    }

    private static bool ShouldSendDiskMetrics(string? theme)
    {
        string normalizedTheme = NormalizeThemeKey(theme);
        return normalizedTheme == "pc_monitor" || normalizedTheme == "gamer";
    }

    public static string NormalizeThemeKey(string? theme)
    {
        if (string.IsNullOrWhiteSpace(theme))
            return "pc_monitor";

        string value = theme.Trim().ToLowerInvariant().Replace("-", "_").Replace(" ", "_");

        return value switch
        {
            "pc" => "pc_monitor",
            "pcmonitor" => "pc_monitor",
            "pc_monitor" => "pc_monitor",
            "minimal" => "minimal_clock",
            "minimalclock" => "minimal_clock",
            "minimal_clock" => "minimal_clock",
            "work" => "work_desk",
            "workdesk" => "work_desk",
            "work_desk" => "work_desk",
            "gamer" => "gamer",
            "animation" => "animation",
            _ => "pc_monitor"
        };
    }

    public static string FormatThemeName(string? theme)
    {
        return NormalizeThemeKey(theme) switch
        {
            "gamer" => "Gamer",
            "minimal_clock" => "Minimal Clock",
            "work_desk" => "Work Desk",
            "animation" => "Animation",
            _ => "PC Monitor"
        };
    }
}
