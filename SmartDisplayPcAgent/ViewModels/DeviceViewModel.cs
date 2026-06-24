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
    private string statusMessage = "Aguardando status do dispositivo...";

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
    private string heapText = "--";

    [ObservableProperty]
    private string flashText = "--";

    [ObservableProperty]
    private string weatherText = "--";

    [ObservableProperty]
    private string temperatureText = "--";

    [ObservableProperty]
    private string displayMetricsText = "--";

    [ObservableProperty]
    private string lastRefreshText = "Never";

    [ObservableProperty]
    private string themeApplyStatusText = "Selecione um tema e aplique no TinyDash.";

    [ObservableProperty]
    private bool isApplyingTheme;

    partial void OnSelectedThemeChanged(ThemeOption? value)
    {
        if (_isSyncingThemeFromDevice || value is null)
            return;

        _hasPendingThemeSelection = value.Key != _currentThemeKey;
        ThemeApplyStatusText = _hasPendingThemeSelection
            ? $"Tema selecionado: {value.DisplayName}"
            : $"Tema ativo no TinyDash: {value.DisplayName}";
    }

    private async Task StartStatusLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            await RefreshStatusInternalAsync(cancellationToken);

            try
            {
                await Task.Delay(10000, cancellationToken);
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
        await RefreshStatusInternalAsync(timeoutCts.Token);
    }

    [RelayCommand]
    private async Task ApplyThemeAsync()
    {
        if (SelectedTheme is null)
        {
            ThemeApplyStatusText = "Nenhum tema selecionado.";
            return;
        }

        IsApplyingTheme = true;
        ThemeApplyStatusText = $"Aplicando {SelectedTheme.DisplayName}...";

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));

        bool applied = await _deviceControlClient.SetThemeAsync(
            State.DisplayIp,
            SelectedTheme.Key,
            timeoutCts.Token);

        if (!applied)
        {
            IsApplyingTheme = false;
            ThemeApplyStatusText = $"Falha ao aplicar {SelectedTheme.DisplayName}.";
            StatusMessage = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                ? $"Falha no POST /theme para {State.DisplayIp}"
                : _deviceControlClient.LastError;
            return;
        }

        ThemeApplyStatusText = $"{SelectedTheme.DisplayName} aplicado. Atualizando status...";
        StatusMessage = "Tema enviado para o TinyDash";

        using var refreshCts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        await RefreshStatusInternalAsync(refreshCts.Token);

        IsApplyingTheme = false;
    }

    private async Task RefreshStatusInternalAsync(CancellationToken cancellationToken)
    {
        var status = await _deviceControlClient.GetStatusAsync(State.DisplayIp, cancellationToken);

        if (status is null)
        {
            Dispatcher.UIThread.Post(() =>
            {
                StatusMessage = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                    ? $"Sem resposta de {State.DisplayIp}"
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

            StatusMessage = "Status atualizado";
            DeviceName = status.Name;
            DeviceMode = status.Mode;
            DeviceIp = status.Ip;
            DeviceSsid = status.Ssid;
            DeviceRssiText = status.Rssi.HasValue ? $"{status.Rssi.Value} dBm" : "--";
            DeviceThemeText = formattedTheme;
            PcOnlineText = status.PcOnline ? "Yes" : "No";
            LastMetricsText = FormatAge(status.LastPcMetricsAgeMs);
            HeapText = FormatBytes(status.Heap);
            FlashText = FormatBytes(status.FlashSize);
            WeatherText = string.IsNullOrWhiteSpace(status.Weather) ? "--" : status.Weather;
            TemperatureText = status.Temperature.HasValue ? $"{status.Temperature.Value:0.0} °C" : "--";
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
                ThemeApplyStatusText = $"Tema ativo no TinyDash: {matchingTheme.DisplayName}";
            }
            else if (SelectedTheme?.Key == normalizedTheme)
            {
                _hasPendingThemeSelection = false;
                ThemeApplyStatusText = $"Tema ativo no TinyDash: {matchingTheme.DisplayName}";
            }
        });
    }

    [RelayCommand]
    private async Task SendTestAsync()
    {
        State.DisplayStatusText = "Testando envio...";
        State.LastPostText = "Testing";

        var snapshot = _metricsService.GetSnapshot();

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(2));

        bool sent = await _displayHttpClient.SendMetricsAsync(
            State.DisplayIp,
            snapshot,
            timeoutCts.Token);

        State.DisplayStatusText = sent
            ? $"Teste enviado para {State.DisplayIp}"
            : $"Falha no teste para {State.DisplayIp}";

        State.DisplayStatusShortText = sent ? "Online" : "Offline";
        State.LastPostText = sent ? "OK · now" : "Failed";
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
            return $"{ageMs.Value} ms ago";

        return $"{ageMs.Value / 1000.0:0.0} s ago";
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
}
