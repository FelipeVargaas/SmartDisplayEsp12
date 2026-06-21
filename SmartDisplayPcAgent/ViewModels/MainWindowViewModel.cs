using System;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SmartDisplayPcAgent.Clients;
using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Services;

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
    private string statusText = "Iniciando coleta...";

    [ObservableProperty]
    private string displayIp = "192.168.0.181";

    [ObservableProperty]
    private bool sendToDisplayEnabled;

    [ObservableProperty]
    private string displayStatusText = "Envio para display desativado";

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

                string displayStatus = DisplayStatusText;

                if (SendToDisplayEnabled)
                {
                    bool sent = await _displayHttpClient.SendMetricsAsync(
                        DisplayIp,
                        snapshot,
                        cancellationToken);

                    displayStatus = sent
                        ? $"Enviado para {DisplayIp}"
                        : $"Falha ao enviar para {DisplayIp}";
                }
                else
                {
                    displayStatus = "Envio para display desativado";
                }

                Dispatcher.UIThread.Post(() =>
                {
                    CpuUsage = cpu;
                    RamUsage = ram;
                    GpuUsage = gpu;
                    StatusText = "Coletando dados locais";
                    DisplayStatusText = displayStatus;
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
            GpuUsage);

        using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(2));

        bool sent = await _displayHttpClient.SendMetricsAsync(
            DisplayIp,
            snapshot,
            timeoutCts.Token);

        DisplayStatusText = sent
            ? $"Teste enviado para {DisplayIp}"
            : $"Falha no teste para {DisplayIp}";
    }

    public void Dispose()
    {
        _cts.Cancel();
        _cts.Dispose();

        _metricsService.Dispose();
        _displayHttpClient.Dispose();
    }
}