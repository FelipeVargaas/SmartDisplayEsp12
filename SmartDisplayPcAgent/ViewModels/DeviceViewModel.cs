using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SmartDisplayPcAgent.Clients;
using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Resources;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.ViewModels;

public partial class DeviceViewModel : ObservableObject, IDisposable
{
    private readonly DeviceControlClient _deviceControlClient = new();
    private readonly DisplayHttpClient _displayHttpClient = new();
    private readonly PcMetricsService _metricsService = new();
    private readonly DeviceLocationSearchService _locationSearchService = new();
    private readonly CancellationTokenSource _cts = new();

    private bool _isSyncingThemeFromDevice;
    private bool _hasPendingThemeSelection;
    private string _currentThemeKey = "pc_monitor";
    private int _telemetryMessageIndex;
    private bool _hasPendingLocationSelection;

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
        _ = Task.Run(() => StartTelemetryMessageLoopAsync(_cts.Token));
    }

    public AgentConnectionState State { get; }

    public IReadOnlyList<ThemeOption> AvailableThemes { get; }

    public ObservableCollection<DeviceLocationOption> LocationResults { get; } = new();

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
    private string telemetryDetailText = S("TargetIpConfigured");

    [ObservableProperty]
    private string locationSearchText = string.Empty;

    [ObservableProperty]
    private DeviceLocationOption? selectedLocation;

    [ObservableProperty]
    private string locationStatusText = S("LocationSearchHint");

    [ObservableProperty]
    private string locationLabelText = S("LocationNotSelected");

    [ObservableProperty]
    private string locationTimezoneText = "--";

    [ObservableProperty]
    private string locationLatitudeText = "--";

    [ObservableProperty]
    private string locationLongitudeText = "--";

    [ObservableProperty]
    private string locationCoordinatesText = "--";

    [ObservableProperty]
    private bool isSearchingLocation;

    [ObservableProperty]
    private bool isApplyingLocation;

    [ObservableProperty]
    private bool hasLocationResults;

    [ObservableProperty]
    private bool isLocationPreviewDialogOpen;

    [ObservableProperty]
    private string locationPreviewPayload = string.Empty;

    [ObservableProperty]
    private bool isSystemDetailsVisible;

    public string SystemDetailsButtonText => IsSystemDetailsVisible ? S("HideDetails") : S("Details");

    [ObservableProperty]
    private bool isConfirmDialogOpen;

    [ObservableProperty]
    private string confirmDialogTitle = string.Empty;

    [ObservableProperty]
    private string confirmDialogMessage = string.Empty;

    [ObservableProperty]
    private string confirmDialogActionText = "Confirmar";

    private string pendingConfirmAction = string.Empty;

    partial void OnIsSystemDetailsVisibleChanged(bool value)
    {
        OnPropertyChanged(nameof(SystemDetailsButtonText));
    }

    partial void OnSelectedLocationChanged(DeviceLocationOption? value)
    {
        if (value is null)
            return;

        LocationLabelText = value.Label;
        LocationTimezoneText = string.IsNullOrWhiteSpace(value.Timezone) ? "--" : value.Timezone;
        LocationLatitudeText = value.Latitude.ToString("0.000000", CultureInfo.InvariantCulture);
        LocationLongitudeText = value.Longitude.ToString("0.000000", CultureInfo.InvariantCulture);
        LocationCoordinatesText = $"{LocationLatitudeText}, {LocationLongitudeText}";
        LocationStatusText = string.Format(S("LocationSelectedFormat"), value.Label);
        _hasPendingLocationSelection = true;
        HasLocationResults = false;
    }

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

    private async Task StartTelemetryMessageLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            UpdateTelemetryDetailText();

            try
            {
                await Task.Delay(4000, cancellationToken);
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
            ApplyWeatherLocationStatus(status);
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
        UpdateTelemetryDetailText();
    }

    [RelayCommand]
    private async Task SearchLocationAsync()
    {
        if (string.IsNullOrWhiteSpace(LocationSearchText))
        {
            LocationStatusText = S("LocationSearchEmpty");
            return;
        }

        IsSearchingLocation = true;
        LocationStatusText = S("LocationSearching");
        _hasPendingLocationSelection = false;
        SelectedLocation = null;
        LocationResults.Clear();
        HasLocationResults = false;

        try
        {
            using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(8));
            string language = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
            var results = await _locationSearchService.SearchAsync(LocationSearchText, language, timeoutCts.Token);

            foreach (var result in results)
                LocationResults.Add(result);

            HasLocationResults = LocationResults.Count > 0;
            if (LocationResults.Count == 0)
                LocationStatusText = S("LocationNoResults");
            else
                LocationStatusText = string.Format(S("LocationResultsFormat"), LocationResults.Count);
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or JsonException)
        {
            LocationStatusText = string.Format(S("LocationSearchFailedFormat"), ex.Message);
        }
        finally
        {
            IsSearchingLocation = false;
        }
    }

    [RelayCommand]
    private async Task ApplyLocationAsync()
    {
        if (SelectedLocation is null)
        {
            LocationStatusText = S("LocationSelectFirst");
            return;
        }

        IsApplyingLocation = true;
        LocationStatusText = S("LocationSending");

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(4));

        bool applied = await _deviceControlClient.SetWeatherLocationAsync(
            State.DisplayIp,
            SelectedLocation,
            timeoutCts.Token);

        if (!applied)
        {
            IsApplyingLocation = false;
            LocationStatusText = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                ? S("LocationSendFailed")
                : _deviceControlClient.LastError;
            return;
        }

        _hasPendingLocationSelection = false;
        LocationStatusText = S("LocationSentRefresh");

        using var refreshCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        await RefreshStatusInternalAsync(refreshCts.Token, waitForSlot: true);

        IsApplyingLocation = false;
    }

    [RelayCommand]
    private void PreviewLocationPayload()
    {
        if (SelectedLocation is null)
        {
            LocationStatusText = S("LocationSelectFirst");
            return;
        }

        var payload = new
        {
            label = SelectedLocation.Label,
            latitude = SelectedLocation.Latitude,
            longitude = SelectedLocation.Longitude,
            timezone = SelectedLocation.Timezone,
        };

        LocationPreviewPayload = JsonSerializer.Serialize(payload, new JsonSerializerOptions
        {
            WriteIndented = true,
        });
        IsLocationPreviewDialogOpen = true;
    }

    [RelayCommand]
    private void CloseLocationPreviewDialog()
    {
        IsLocationPreviewDialogOpen = false;
    }

    [RelayCommand]
    private void ToggleSystemDetails()
    {
        IsSystemDetailsVisible = !IsSystemDetailsVisible;
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
    private async Task ConfirmDialogAsync()
    {
        if (pendingConfirmAction == "reset_wifi")
            StatusMessage = S("ResetWifiConfirmed");
        else if (pendingConfirmAction == "firmware_update")
            await PrepareFirmwareUpdateAsync();

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
        _locationSearchService.Dispose();
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

    private void ApplyWeatherLocationStatus(DeviceStatusSnapshot status)
    {
        if (_hasPendingLocationSelection ||
            string.IsNullOrWhiteSpace(status.WeatherLocationLabel) ||
            !status.WeatherLatitude.HasValue ||
            !status.WeatherLongitude.HasValue)
        {
            return;
        }

        LocationLabelText = status.WeatherLocationLabel;
        LocationTimezoneText = string.IsNullOrWhiteSpace(status.WeatherTimezone) ? "--" : status.WeatherTimezone;
        LocationLatitudeText = status.WeatherLatitude.Value.ToString("0.000000", CultureInfo.InvariantCulture);
        LocationLongitudeText = status.WeatherLongitude.Value.ToString("0.000000", CultureInfo.InvariantCulture);
        LocationCoordinatesText = $"{LocationLatitudeText}, {LocationLongitudeText}";
        LocationStatusText = S("LocationStatusFromDevice");
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

    private async Task PrepareFirmwareUpdateAsync()
    {
        if (string.IsNullOrWhiteSpace(State.DisplayIp))
        {
            StatusMessage = S("ConfigureTargetIpFirst");
            return;
        }

        StatusMessage = S("FirmwareUpdatePreparing");

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(4));
        bool prepared = await _deviceControlClient.PrepareFirmwareUpdateAsync(
            State.DisplayIp,
            timeoutCts.Token);

        if (!prepared)
        {
            StatusMessage = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                ? S("FirmwareUpdatePrepareFailed")
                : _deviceControlClient.LastError;
            return;
        }

        string updateUrl = NormalizeDeviceUrl(State.DisplayIp) + "update";
        Process.Start(new ProcessStartInfo(updateUrl) { UseShellExecute = true });
        StatusMessage = string.Format(S("FirmwareUpdateMaintenanceStartedFormat"), updateUrl);
    }

    private void UpdateTelemetryDetailText()
    {
        string text = _telemetryMessageIndex++ % 2 == 0
            ? State.DisplayStatusText
            : string.Format(S("TelemetryTargetFormat"), State.DisplayIp);

        if (string.IsNullOrWhiteSpace(text))
            text = S("TargetIpConfigured");

        Dispatcher.UIThread.Post(() => TelemetryDetailText = text);
    }

    private static string S(string name) => Strings.Get(name);
}
