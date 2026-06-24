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

public partial class MainWindowViewModel : ObservableObject, IDisposable
{
    private readonly PcMetricsService _metricsService = new();
    private readonly DisplayHttpClient _displayHttpClient = new();
    private readonly CancellationTokenSource _cts = new();

    [ObservableProperty]
    private double cpuUsage;

    [ObservableProperty]
    private double ramUsage;

    [ObservableProperty]
    private double gpuUsage;

    [ObservableProperty]
    private double? gpuTemperature;

    [ObservableProperty]
    private string gpuTemperatureDisplayText = "GPU TEMP  --";

    [ObservableProperty]
    private string gpuTemperatureDisplayCompactText = "--";

    [ObservableProperty]
    private string statusText = "Iniciando coleta...";

    [ObservableProperty]
    private string displayIp = "192.168.0.181";

    [ObservableProperty]
    private bool sendToDisplayEnabled;

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
    private string diskDisplayText = "---  0%";

    [ObservableProperty]
    private string disksText = "Nenhum disco detectado";

    [ObservableProperty]
    private string activeThemeText = "Tema ESP: não consultado";

    [ObservableProperty]
    private string gamerGameText = "Game: -";

    [ObservableProperty]
    private string gamerGameValueText = "--";

    [ObservableProperty]
    private string gamerFpsText = "FPS: --";

    [ObservableProperty]
    private string gamerFpsValueText = "--";

    [ObservableProperty]
    private string gamerFrametimeText = "Frametime: --";

    [ObservableProperty]
    private string gamerFrametimeValueText = "--";

    [ObservableProperty]
    private string gamerSourceText = "Source: RTSS OFF";

    [ObservableProperty]
    private string gamerSourceValueText = "RTSS OFF";

    public MainWindowViewModel()
    {
        _ = Task.Run(() => StartMetricsLoopAsync(_cts.Token));
    }

    private async Task StartMetricsLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            try
            {
                var snapshot = _metricsService.GetSnapshot();

                double cpu = Math.Round(snapshot.CpuUsage);
                double ram = Math.Round(snapshot.RamUsage);
                double gpu = Math.Round(snapshot.GpuUsage);

                double? gpuTemperature = snapshot.GpuTemperature.HasValue
                    ? Math.Round(snapshot.GpuTemperature.Value)
                    : null;

                string gpuTemperatureDisplayText = FormatTemperature(snapshot.GpuTemperature);
                string gpuTemperatureDisplayCompactText = FormatTemperatureCompact(snapshot.GpuTemperature);

                double disk = Math.Round(snapshot.DiskUsage);
                string diskLabel = snapshot.DiskLabel;
                string diskDisplayText = $"{diskLabel}  {disk:0}%";
                string disksText = BuildDisksText(snapshot.Disks);

                string gamerGameText = FormatGame(snapshot.Game);
                string gamerGameValueText = FormatGameValue(snapshot.Game);
                string gamerFpsText = FormatFps(snapshot.Fps);
                string gamerFpsValueText = FormatFpsValue(snapshot.Fps);
                string gamerFrametimeText = FormatFrametime(snapshot.Frametime);
                string gamerFrametimeValueText = FormatFrametimeValue(snapshot.Frametime);
                string gamerSourceText = FormatSource(snapshot.Source);
                string gamerSourceValueText = FormatSourceValue(snapshot.Source);

                string displayStatus = DisplayStatusText;
                string displayStatusShort = DisplayStatusShortText;
                string espHeaderStatus = EspHeaderStatusText;
                string lastPostText = LastPostText;

                if (SendToDisplayEnabled)
                {
                    bool sent = await _displayHttpClient.SendMetricsAsync(
                        DisplayIp,
                        snapshot,
                        cancellationToken);

                    displayStatus = sent
                        ? $"Enviado para {DisplayIp}"
                        : $"Falha ao enviar para {DisplayIp}";

                    displayStatusShort = sent ? "Online" : "Offline";
                    espHeaderStatus = sent ? "ESP ONLINE" : "ESP OFFLINE";
                    lastPostText = sent ? "OK · 1s ago" : "Failed";
                }
                else
                {
                    displayStatus = "Envio para display desativado";
                    displayStatusShort = "Paused";
                    espHeaderStatus = "ESP READY";
                    lastPostText = "Disabled";
                }

                Dispatcher.UIThread.Post(() =>
                {
                    CpuUsage = cpu;
                    RamUsage = ram;
                    GpuUsage = gpu;
                    GpuTemperature = gpuTemperature;
                    GpuTemperatureDisplayText = gpuTemperatureDisplayText;
                    GpuTemperatureDisplayCompactText = gpuTemperatureDisplayCompactText;
                    StatusText = "Coletando dados locais";
                    DisplayStatusText = displayStatus;
                    DisplayStatusShortText = displayStatusShort;
                    EspHeaderStatusText = espHeaderStatus;
                    LastPostText = lastPostText;
                    DiskUsage = disk;
                    DiskLabel = diskLabel;
                    DiskDisplayText = diskDisplayText;
                    DisksText = disksText;
                    ActiveThemeText = "PC Monitor";
                    GamerGameText = gamerGameText;
                    GamerGameValueText = gamerGameValueText;
                    GamerFpsText = gamerFpsText;
                    GamerFpsValueText = gamerFpsValueText;
                    GamerFrametimeText = gamerFrametimeText;
                    GamerFrametimeValueText = gamerFrametimeValueText;
                    GamerSourceText = gamerSourceText;
                    GamerSourceValueText = gamerSourceValueText;
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
            GpuTemperature: GpuTemperature);

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(2));

        bool sent = await _displayHttpClient.SendMetricsAsync(
            DisplayIp,
            snapshot,
            timeoutCts.Token);

        DisplayStatusText = sent
            ? $"Teste enviado para {DisplayIp}"
            : $"Falha no teste para {DisplayIp}";

        DisplayStatusShortText = sent ? "Online" : "Offline";
        EspHeaderStatusText = sent ? "ESP ONLINE" : "ESP OFFLINE";
        LastPostText = sent ? "OK · now" : "Failed";
    }

    public void Dispose()
    {
        _cts.Cancel();
        _cts.Dispose();

        _metricsService.Dispose();
        _displayHttpClient.Dispose();
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
                    $"R {d.ReadMbPerSecond:0.0} MB/s  " +
                    $"W {d.WriteMbPerSecond:0.0} MB/s"));
    }

    private static string FormatTemperature(double? temperature)
    {
        return temperature.HasValue
            ? $"GPU TEMP  {temperature.Value:0}°C"
            : "GPU TEMP  --";
    }

    private static string FormatTemperatureCompact(double? temperature)
    {
        return temperature.HasValue
            ? $"{temperature.Value:0}°C"
            : "--";
    }

    private static string FormatGame(string? game)
    {
        return string.IsNullOrWhiteSpace(game)
            ? "Game: -"
            : $"Game: {game.Trim()}";
    }

    private static string FormatGameValue(string? game)
    {
        return string.IsNullOrWhiteSpace(game)
            ? "--"
            : game.Trim();
    }

    private static string FormatFps(int? fps)
    {
        return fps.HasValue
            ? $"FPS: {fps.Value:0}"
            : "FPS: --";
    }

    private static string FormatFpsValue(int? fps)
    {
        return fps.HasValue
            ? $"{fps.Value:0}"
            : "--";
    }

    private static string FormatFrametime(double? frametime)
    {
        return frametime.HasValue
            ? $"Frametime: {frametime.Value:0.0} ms"
            : "Frametime: --";
    }

    private static string FormatFrametimeValue(double? frametime)
    {
        return frametime.HasValue
            ? $"{frametime.Value:0.0} ms"
            : "--";
    }

    private static string FormatSource(string? source)
    {
        return string.IsNullOrWhiteSpace(source)
            ? "Source: RTSS OFF"
            : $"Source: {source.Trim()}";
    }

    private static string FormatSourceValue(string? source)
    {
        return string.IsNullOrWhiteSpace(source)
            ? "RTSS OFF"
            : source.Trim();
    }
}
