using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SmartDisplayPcAgent.Clients;
using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Resources;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.ViewModels;

public partial class DeviceViewModel : ObservableObject, IDisposable
{
    private readonly DeviceControlClient _deviceControlClient = new();
    private readonly DisplayHttpClient _displayHttpClient = new();
    private readonly PcMetricsService _metricsService = new();
    private readonly CancellationTokenSource _cts = new();

    private bool _isSyncingThemeFromDevice;
    private bool _hasPendingThemeSelection;
    private string _currentThemeKey = "pc_monitor";

    public DeviceViewModel(AgentConnectionState state)
    {
        State = state;
        AvailableThemes = new[]
        {
            new ThemeOption("pc_monitor", "PC Monitor"),
            new ThemeOption("gamer", "Gamer"),
            new ThemeOption("minimal_clock", "Minimal Clock"),
            new ThemeOption("work_desk", "Work Desk"),
            new ThemeOption("animation", "Animation"),
        };

        SelectedTheme = AvailableThemes[0];
        _ = Task.Run(() => StartStatusLoopAsync(_cts.Token));
    }

    public AgentConnectionState State { get; }

    public IReadOnlyList<ThemeOption> AvailableThemes { get; }

    [ObservableProperty]
    private ThemeOption? selectedTheme;

    [ObservableProperty]
    private string statusMessage = S("DeviceWaitingStatus");

    [ObservableProperty]
    private string deviceName = "TinyDash";

    [ObservableProperty]
    private string deviceMode = "--";

    [ObservableProperty]
    private string deviceIp = "--";

    [ObservableProperty]
    private string deviceSsid = "--";

    [ObservableProperty]
    private string deviceRssiText = "--";

    [ObservableProperty]
    private string deviceThemeText = "PC Monitor";

    [ObservableProperty]
    private string pcOnlineText = "--";

    [ObservableProperty]
    private string lastMetricsText = "--";

    [ObservableProperty]
    private string uptimeText = "--";

    [ObservableProperty]
    private string resetReasonText = "--";

    [ObservableProperty]
    private string resetInfoText = "--";

    [ObservableProperty]
    private string restartIntentText = "--";

    [ObservableProperty]
    private string lastCheckpointText = "--";

    [ObservableProperty]
    private string heapText = "--";

    [ObservableProperty]
    private string heapFragmentationText = "--";

    [ObservableProperty]
    private string maxFreeBlockText = "--";

    [ObservableProperty]
    private string flashText = "--";

    [ObservableProperty]
    private string weatherText = "--";

    [ObservableProperty]
    private string temperatureText = "--";

    [ObservableProperty]
    private string weatherStatusText = "--";

    [ObservableProperty]
    private string displayMetricsText = "--";

    [ObservableProperty]
    private string lastRefreshText = S("Never");

    [ObservableProperty]
    private string themeApplyStatusText = S("SelectThemeApply");

    [ObservableProperty]
    private bool isApplyingTheme;

    [ObservableProperty]
    private bool isConfirmDialogOpen;

    [ObservableProperty]
    private string confirmDialogTitle = string.Empty;

    [ObservableProperty]
    private string confirmDialogMessage = string.Empty;

    [ObservableProperty]
    private string confirmDialogActionText = "Confirmar";

    private string pendingConfirmAction = string.Empty;

    partial void OnSelectedThemeChanged(ThemeOption? value)
    {
        if (_isSyncingThemeFromDevice || value is null)
            return;

        _hasPendingThemeSelection = value.Key != _currentThemeKey;
        ThemeApplyStatusText = _hasPendingThemeSelection
            ? string.Format(S("ThemeSelectedFormat"), value.DisplayName)
            : string.Format(S("ThemeActiveFormat"), value.DisplayName);
    }

    private async Task StartStatusLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            await RefreshStatusInternalAsync(cancellationToken);

            try
            {
                await Task.Delay(30000, cancellationToken);
            }
            catch (TaskCanceledException)
            {
                break;
            }
        }
    }

    [RelayCommand]
    private async Task RefreshStatusAsync()
    {
        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        await RefreshStatusInternalAsync(timeoutCts.Token, waitForSlot: true);
    }

    [RelayCommand]
    private async Task ApplyThemeAsync()
    {
        if (SelectedTheme is null)
        {
            ThemeApplyStatusText = S("NoThemeSelected");
            return;
        }

        IsApplyingTheme = true;
        ThemeApplyStatusText = string.Format(S("ApplyingThemeFormat"), SelectedTheme.DisplayName);

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));

        bool applied = await _deviceControlClient.SetThemeAsync(
            State.DisplayIp,
            SelectedTheme.Key,
            timeoutCts.Token);

        if (!applied)
        {
            IsApplyingTheme = false;
            ThemeApplyStatusText = string.Format(S("ThemeApplyFailedFormat"), SelectedTheme.DisplayName);
            StatusMessage = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                ? string.Format(S("ThemePostFailedFormat"), State.DisplayIp)
                : _deviceControlClient.LastError;
            return;
        }

        ThemeApplyStatusText = string.Format(S("ThemeAppliedRefreshFormat"), SelectedTheme.DisplayName);
        StatusMessage = S("ThemeSentTinyDash");
        State.ActiveThemeKey = SelectedTheme.Key;

        using var refreshCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        await RefreshStatusInternalAsync(refreshCts.Token, waitForSlot: true);

        IsApplyingTheme = false;
    }

    private async Task RefreshStatusInternalAsync(CancellationToken cancellationToken, bool waitForSlot = false)
    {
        var status = await _deviceControlClient.GetStatusAsync(State.DisplayIp, cancellationToken, waitForSlot);

        if (status is null)
        {
            if (_deviceControlClient.LastRequestSkipped)
                return;

            Dispatcher.UIThread.Post(() =>
            {
                StatusMessage = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                    ? string.Format(S("NoResponseFormat"), State.DisplayIp)
                    : _deviceControlClient.LastError;
                LastRefreshText = DateTime.Now.ToString("HH:mm:ss");
            });
            return;
        }

        ApplyStatus(status);
    }

    private void ApplyStatus(DeviceStatusSnapshot status)
    {
        Dispatcher.UIThread.Post(() =>
        {
            string normalizedTheme = DashboardViewModel.NormalizeThemeKey(status.Theme);
            string formattedTheme = DashboardViewModel.FormatThemeName(normalizedTheme);
            _currentThemeKey = normalizedTheme;
            State.ActiveThemeKey = normalizedTheme;

            StatusMessage = S("DeviceStatusUpdated");
            DeviceName = status.Name;
            DeviceMode = status.Mode;
            DeviceIp = status.Ip;
            DeviceSsid = status.Ssid;
            DeviceRssiText = status.Rssi.HasValue ? $"{status.Rssi.Value} dBm" : "--";
            DeviceThemeText = formattedTheme;
            PcOnlineText = status.PcOnline ? S("Yes") : S("No");
            LastMetricsText = FormatAge(status.LastPcMetricsAgeMs);
            UptimeText = FormatDuration(status.UptimeMs);
            ResetReasonText = FormatStatusText(status.ResetReason);
            ResetInfoText = FormatStatusText(status.ResetInfo);
            RestartIntentText = FormatStatusText(status.RestartIntent);
            LastCheckpointText = FormatStatusText(status.LastCheckpoint);
            HeapText = FormatBytes(status.Heap);
            HeapFragmentationText = status.HeapFragmentation.HasValue ? $"{status.HeapFragmentation.Value}%" : "--";
            MaxFreeBlockText = FormatBytes(status.MaxFreeBlockSize);
            FlashText = FormatBytes(status.FlashSize);
            WeatherText = string.IsNullOrWhiteSpace(status.Weather) ? "--" : status.Weather;
            TemperatureText = status.Temperature.HasValue ? $"{status.Temperature.Value:0.0} °C" : "--";
            WeatherStatusText = FormatStatusText(status.WeatherStatus);
            DisplayMetricsText =
                $"CPU {status.Cpu:0}%  RAM {status.Ram:0}%  GPU {status.Gpu:0}%  {status.DiskLabel} {status.Disk:0}%";
            LastRefreshText = DateTime.Now.ToString("HH:mm:ss");

            var matchingTheme = AvailableThemes.FirstOrDefault(t => t.Key == normalizedTheme) ?? AvailableThemes[0];

            if (!_hasPendingThemeSelection || IsApplyingTheme)
            {
                _isSyncingThemeFromDevice = true;
                SelectedTheme = matchingTheme;
                _isSyncingThemeFromDevice = false;
                _hasPendingThemeSelection = false;
                ThemeApplyStatusText = string.Format(S("ThemeActiveFormat"), matchingTheme.DisplayName);
            }
            else if (SelectedTheme?.Key == normalizedTheme)
            {
                _hasPendingThemeSelection = false;
                ThemeApplyStatusText = string.Format(S("ThemeActiveFormat"), matchingTheme.DisplayName);
            }
        });
    }

    [RelayCommand]
    private async Task SendTestAsync()
    {
        State.DisplayStatusText = S("TestingSend");
        State.LastPostText = "Testing";

        var snapshot = _metricsService.GetSnapshot();

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(2));

        bool sent = await _displayHttpClient.SendMetricsAsync(
            State.DisplayIp,
            snapshot,
            timeoutCts.Token,
            waitForSlot: true);

        State.DisplayStatusText = sent
            ? string.Format(S("TestSentFormat"), State.DisplayIp)
            : string.Format(S("TestFailedFormat"), State.DisplayIp);

        State.DisplayStatusShortText = sent ? S("StatusOnline") : S("StatusOffline");
        State.LastPostText = sent ? S("OkNow") : S("StatusFailed");
    }

    [RelayCommand]
    private async Task OpenServerAsync()
    {
        if (string.IsNullOrWhiteSpace(State.DisplayIp))
        {
            StatusMessage = S("ConfigureTargetIpFirst");
            return;
        }

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        var status = await _deviceControlClient.GetStatusAsync(State.DisplayIp, timeoutCts.Token, waitForSlot: true);

        if (status is null)
        {
            StatusMessage = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                ? string.Format(S("NoResponseFormat"), State.DisplayIp)
                : _deviceControlClient.LastError;
            return;
        }

        string url = NormalizeDeviceUrl(State.DisplayIp);
        Process.Start(new ProcessStartInfo(url) { UseShellExecute = true });
        StatusMessage = string.Format(S("OpeningFormat"), url);
    }

    [RelayCommand]
    private void RequestResetWifi()
    {
        pendingConfirmAction = "reset_wifi";
        ConfirmDialogTitle = S("ResetWifiTitle");
        ConfirmDialogMessage = S("ResetWifiMessage");
        ConfirmDialogActionText = S("ResetSavedWifi");
        IsConfirmDialogOpen = true;
    }

    [RelayCommand]
    private void RequestFirmwareUpdate()
    {
        pendingConfirmAction = "firmware_update";
        ConfirmDialogTitle = S("OpenFirmwareUpdateTitle");
        ConfirmDialogMessage = S("OpenFirmwareUpdateMessage");
        ConfirmDialogActionText = S("Continue");
        IsConfirmDialogOpen = true;
    }

    [RelayCommand]
    private void CancelConfirmDialog()
    {
        pendingConfirmAction = string.Empty;
        IsConfirmDialogOpen = false;
    }

    [RelayCommand]
    private void ConfirmDialog()
    {
        if (pendingConfirmAction == "reset_wifi")
            StatusMessage = S("ResetWifiConfirmed");
        else if (pendingConfirmAction == "firmware_update")
            StatusMessage = S("FirmwareUpdateConfirmed");

        pendingConfirmAction = string.Empty;
        IsConfirmDialogOpen = false;
    }

    public void Dispose()
    {
        _cts.Cancel();
        _cts.Dispose();
        _deviceControlClient.Dispose();
        _displayHttpClient.Dispose();
        _metricsService.Dispose();
    }

    private static string FormatAge(long? ageMs)
    {
        if (!ageMs.HasValue)
            return "--";

        if (ageMs.Value < 1000)
            return string.Format(S("MsAgoFormat"), ageMs.Value);

        return string.Format(S("SecondsAgoFormat"), ageMs.Value / 1000.0);
    }

    private static string FormatDuration(long? durationMs)
    {
        if (!durationMs.HasValue)
            return "--";

        long totalSeconds = Math.Max(0, durationMs.Value / 1000);
        long hours = totalSeconds / 3600;
        long minutes = totalSeconds % 3600 / 60;
        long seconds = totalSeconds % 60;

        return $"{hours}h {minutes}m {seconds}s";
    }

    private static string FormatStatusText(string? value)
    {
        return string.IsNullOrWhiteSpace(value) ? "--" : value.Trim();
    }

    private static string FormatBytes(long? bytes)
    {
        if (!bytes.HasValue)
            return "--";

        double value = bytes.Value;

        if (value >= 1024 * 1024)
            return $"{value / (1024 * 1024):0.0} MB";

        if (value >= 1024)
            return $"{value / 1024:0.0} KB";

        return $"{value:0} B";
    }

    private static string NormalizeDeviceUrl(string displayIp)
    {
        string value = displayIp.Trim();

        if (!value.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
            !value.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            value = "http://" + value;
        }

        return value.TrimEnd('/') + "/";
    }

    private static string S(string name) => Strings.Get(name);
}
