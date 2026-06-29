using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SmartDisplayPcAgent.Clients;
using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Resources;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.ViewModels;

public partial class DashboardViewModel : ObservableObject, IDisposable
{
    private readonly PcMetricsService _metricsService = new();
    private readonly DisplayHttpClient _displayHttpClient = new();
    private readonly DeviceControlClient _deviceControlClient = new();
    private readonly AnimationImageService _animationImageService = new();
    private readonly HardwareInfoService _hardwareInfoService = new();
    private readonly GamerTelemetryService _gamerTelemetryService = new();
    private readonly GameAliasService _gameAliasService = new();
    private readonly CancellationTokenSource _cts = new();

    private DateTime _lastDeviceStatusRefresh = DateTime.MinValue;
    private DateTime _metricsBackoffUntil = DateTime.MinValue;
    private bool _isDeviceStatusRefreshRunning;
    private string _activeThemeKey = "pc_monitor";
    private byte[]? _animationSourceBytes;
    private byte[]? _animationImagePayload;
    private readonly List<Bitmap> _animationPreviewFrames = [];
    private DispatcherTimer? _animationPreviewTimer;
    private int _animationPreviewFrameIndex;
    private bool _animationImageUploaded;

    public DashboardViewModel(AgentConnectionState state)
    {
        State = state;
        LoadHardwareSummary();
        UpdateThemePanel(_activeThemeKey);
        _ = Task.Run(() => RefreshDeviceStatusForDashboardAsync(_cts.Token));
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
    private string statusText = S("StatusStartingCollection");

    [ObservableProperty]
    private string displayStatusText = S("StatusSendDisabled");

    [ObservableProperty]
    private string displayStatusShortText = S("StatusPaused");

    [ObservableProperty]
    private string espHeaderStatusText = S("EspReady");

    [ObservableProperty]
    private string lastPostText = S("StatusDisabled");

    [ObservableProperty]
    private double diskUsage;

    [ObservableProperty]
    private string diskLabel = "---";

    [ObservableProperty]
    private string disksText = S("NoDisksDetected");

    [ObservableProperty]
    private string activeThemeText = "PC Monitor";

    [ObservableProperty]
    private string deviceHeaderText = S("NoDeviceStatusYet");

    [ObservableProperty]
    private string themePanelTitle = "PC MONITOR";

    [ObservableProperty]
    private string themePanelBodyText = "Ponte de telemetria ativa";

    [ObservableProperty]
    private string themePanelStatusText = "Aguardando primeiro status do dispositivo";

    [ObservableProperty]
    private string cpuNameText = S("DetectingCpu");

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
    private string memoryChannelText = S("Unknown");

    [ObservableProperty]
    private string gpuNameText = S("DetectingGpu");

    [ObservableProperty]
    private string gpuVramText = "--";

    [ObservableProperty]
    private string gpuClockText = "--";

    [ObservableProperty]
    private string pcSummaryGpuTemperatureText = "--";

    [ObservableProperty]
    private string storageNameText = S("Storage");

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

    [ObservableProperty]
    private bool isGameNamesDialogOpen;

    [ObservableProperty]
    private bool isAnimationImageDialogOpen;

    [ObservableProperty]
    private bool isGamerThemeActive;

    [ObservableProperty]
    private bool isAnimationThemeActive;

    [ObservableProperty]
    private string animationSelectedFileText = S("NoImageSelected");

    [ObservableProperty]
    private string animationImageDetailsText = S("AnimationDetails");

    [ObservableProperty]
    private string animationImageStatusText = S("AnimationChoosePrepare");

    [ObservableProperty]
    private bool canUploadAnimationImage;

    [ObservableProperty]
    private bool isUploadingAnimationImage;

    [ObservableProperty]
    private Bitmap? animationPreviewImage;

    [ObservableProperty]
    private bool hasAnimationPreviewImage;

    [ObservableProperty]
    private double animationCropZoom = 1.0;

    [ObservableProperty]
    private double animationCropOffsetX;

    [ObservableProperty]
    private double animationCropOffsetY;

    [ObservableProperty]
    private string gameAliasProcessText = "--";

    [ObservableProperty]
    private string gameAliasDisplayNameText = "";

    [ObservableProperty]
    private string gameAliasStatusText = S("NoGameDetectedYet");

    private async Task RefreshDeviceStatusForDashboardAsync(CancellationToken cancellationToken)
    {
        if (_isDeviceStatusRefreshRunning)
            return;

        _lastDeviceStatusRefresh = DateTime.Now;
        _isDeviceStatusRefreshRunning = true;

        try
        {
            var deviceStatus = await _deviceControlClient.GetStatusAsync(State.DisplayIp, cancellationToken, waitForSlot: true);

            if (deviceStatus is not null)
            {
                _activeThemeKey = NormalizeThemeKey(deviceStatus.Theme);
                State.ActiveThemeKey = _activeThemeKey;
                _animationImageUploaded = deviceStatus.AnimationImage;
                if (deviceStatus.LowHeap)
                    _metricsBackoffUntil = DateTime.Now.AddSeconds(10);
                string themeText = FormatThemeName(_activeThemeKey);
                string deviceHeader = $"{themeText} - {deviceStatus.Ip}";

                Dispatcher.UIThread.Post(() =>
                {
                    ActiveThemeText = themeText;
                    DeviceHeaderText = deviceHeader;
                    EspHeaderStatusText = S("EspOnline");
                    UpdateThemePanel(_activeThemeKey);
                });
            }
            else
            {
                bool hasRecentEspSuccess = DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45));
                string espHeaderStatus = _deviceControlClient.LastRequestSkipped
                    ? hasRecentEspSuccess ? S("EspOnline") : S("EspWait")
                    : hasRecentEspSuccess ? S("EspUnstable") : S("EspOffline");

                Dispatcher.UIThread.Post(() =>
                {
                    DeviceHeaderText = string.Format(S("NoResponseFormat"), State.DisplayIp);
                    EspHeaderStatusText = espHeaderStatus;
                });
            }
        }
        finally
        {
            _isDeviceStatusRefreshRunning = false;
        }
    }

    private async Task StartMetricsLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            try
            {
                var snapshot = _metricsService.GetSnapshot();
                var gamerTelemetry = _gamerTelemetryService.GetSnapshot();
                string gamerProcessName = gamerTelemetry.Game;
                string gamerDisplayName = _gameAliasService.Resolve(gamerProcessName);

                snapshot = snapshot with
                {
                    Game = gamerDisplayName,
                    Fps = gamerTelemetry.Fps,
                    Frametime = gamerTelemetry.Frametime,
                    Source = gamerTelemetry.Source
                };

                string gamerGameText = string.IsNullOrWhiteSpace(gamerDisplayName) ? "--" : gamerDisplayName;
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

                string gpuTemperatureDisplayCompactText = FormatTemperatureAside(snapshot.GpuTemperature);

                double disk = Math.Round(snapshot.DiskUsage);
                string diskLabel = snapshot.DiskLabel;
                string disksText = BuildDisksText(snapshot.Disks);

                string displayStatus = DisplayStatusText;
                string displayStatusShort = DisplayStatusShortText;
                string espHeaderStatus = EspHeaderStatusText;
                string lastPostText = LastPostText;
                string activeThemeKey = NormalizeThemeKey(State.ActiveThemeKey);
                bool metricsBackoffActive = DateTime.Now < _metricsBackoffUntil;

                if (State.SendToDisplayEnabled && ShouldSendMetricsForTheme(activeThemeKey) && !metricsBackoffActive)
                {
                    bool sent = await _displayHttpClient.SendMetricsAsync(
                        State.DisplayIp,
                        snapshot,
                        cancellationToken,
                        includeDisk: ShouldSendDiskMetrics(activeThemeKey),
                        includeGamerTelemetry: activeThemeKey == "gamer");

                    if (_displayHttpClient.LastSendSkipped)
                    {
                        bool hasRecentEspSuccess = DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45));
                        displayStatus = S("EspBusySkipped");
                        displayStatusShort = hasRecentEspSuccess ? S("StatusOnline") : S("StatusWaitingShort");
                        espHeaderStatus = hasRecentEspSuccess ? S("EspOnline") : S("EspWait");
                        lastPostText = S("StatusSkipped");
                    }
                    else
                    {

                    displayStatus = sent
                        ? string.Format(S("SentToFormat"), State.DisplayIp)
                        : string.Format(S("SendFailedFormat"), State.DisplayIp);

                    displayStatusShort = sent ? S("StatusOnline") : DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45)) ? S("StatusUnstable") : S("StatusOffline");
                    espHeaderStatus = sent ? S("EspOnline") : DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45)) ? S("EspUnstable") : S("EspOffline");
                    lastPostText = sent ? S("OkOneSecondAgo") : S("StatusFailed");
                    if (_displayHttpClient.LastLowHeap)
                    {
                        _metricsBackoffUntil = DateTime.Now.AddSeconds(10);
                        displayStatus = S("EspLowHeapWaiting");
                        displayStatusShort = S("StatusBackoff");
                        espHeaderStatus = S("EspBusy");
                        lastPostText = S("StatusLowHeap");
                    }
                }
                }
                else if (State.SendToDisplayEnabled && ShouldSendMetricsForTheme(activeThemeKey) && metricsBackoffActive)
                {
                    int remainingSeconds = Math.Max(1, (int)Math.Ceiling((_metricsBackoffUntil - DateTime.Now).TotalSeconds));
                    displayStatus = string.Format(S("EspLowHeapResumeFormat"), remainingSeconds);
                    displayStatusShort = S("StatusBackoff");
                    espHeaderStatus = S("EspBusy");
                    lastPostText = S("StatusBackoff");
                }
                else
                {
                    displayStatus = State.SendToDisplayEnabled
                        ? string.Format(S("ThemeNoTelemetryFormat"), FormatThemeName(activeThemeKey))
                        : S("StatusSendDisabled");
                    displayStatusShort = State.SendToDisplayEnabled ? S("StatusIdle") : S("StatusPaused");
                    lastPostText = State.SendToDisplayEnabled ? S("StatusIdle") : S("StatusDisabled");
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
                            State.ActiveThemeKey = _activeThemeKey;
                            _animationImageUploaded = deviceStatus.AnimationImage;
                            if (deviceStatus.LowHeap)
                                _metricsBackoffUntil = DateTime.Now.AddSeconds(10);
                            string themeText = FormatThemeName(_activeThemeKey);
                            string deviceHeader = $"{themeText} - {deviceStatus.Ip}";
                            espHeaderStatus = S("EspOnline");

                            Dispatcher.UIThread.Post(() =>
                            {
                                ActiveThemeText = themeText;
                                DeviceHeaderText = deviceHeader;
                                EspHeaderStatusText = S("EspOnline");
                                UpdateThemePanel(_activeThemeKey);
                            });
                        }
                        else
                        {
                            bool hasRecentEspSuccess = DisplayRequestCoordinator.HasRecentSuccess(TimeSpan.FromSeconds(45));
                            espHeaderStatus = _deviceControlClient.LastRequestSkipped
                                ? hasRecentEspSuccess ? S("EspOnline") : S("EspWait")
                                : hasRecentEspSuccess ? S("EspUnstable") : S("EspOffline");

                            Dispatcher.UIThread.Post(() =>
                            {
                                DeviceHeaderText = string.Format(S("NoResponseFormat"), State.DisplayIp);
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
                    GameAliasProcessText = string.IsNullOrWhiteSpace(gamerProcessName) ? "--" : gamerProcessName;
                    if (!IsGameNamesDialogOpen)
                        GameAliasDisplayNameText = _gameAliasService.GetAlias(gamerProcessName);
                    GamerFpsText = gamerFpsText;
                    GamerFrametimeText = gamerFrametimeText;
                    GamerSourceText = gamerSourceText;
                    GamerStatusText = gamerStatusText;
                    StatusText = S("StatusCollectingLocalData");
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
                    StatusText = string.Format(S("CollectionErrorFormat"), ex.Message);
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
        DisplayStatusText = S("TestingSend");

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
            ? string.Format(S("TestSentFormat"), State.DisplayIp)
            : string.Format(S("TestFailedFormat"), State.DisplayIp);

        DisplayStatusShortText = sent ? S("StatusOnline") : S("StatusOffline");
        EspHeaderStatusText = sent ? S("EspOnline") : S("EspOffline");
        LastPostText = sent ? S("OkNow") : S("StatusFailed");

        State.DisplayStatusText = DisplayStatusText;
        State.DisplayStatusShortText = DisplayStatusShortText;
        State.LastPostText = LastPostText;
    }

    [RelayCommand]
    private void OpenGameNames()
    {
        GameAliasDisplayNameText = _gameAliasService.GetAlias(GameAliasProcessText);
        GameAliasStatusText = GameAliasProcessText == "--"
            ? S("NoGameDetectedYet")
            : $"Editando alias para {GameAliasProcessText}";
        IsGameNamesDialogOpen = true;
    }

    [RelayCommand]
    private void OpenAnimationImage()
    {
        IsAnimationImageDialogOpen = true;
        StartAnimationPreviewTimer();
        AnimationImageStatusText = _animationImagePayload is null
            ? S("AnimationChoosePrepare")
            : "Imagem pronta para envio.";
    }

    [RelayCommand]
    private void CloseAnimationImage()
    {
        StopAnimationPreviewTimer();
        IsAnimationImageDialogOpen = false;
    }

    partial void OnAnimationCropZoomChanged(double value) => UpdateAnimationFramingPreview();

    partial void OnAnimationCropOffsetXChanged(double value) => UpdateAnimationFramingPreview();

    partial void OnAnimationCropOffsetYChanged(double value) => UpdateAnimationFramingPreview();

    [RelayCommand]
    private async Task PickAnimationImageAsync()
    {
        var topLevel = GetMainTopLevel();
        if (topLevel is null)
        {
            AnimationImageStatusText = "Janela principal indisponível.";
            return;
        }

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = S("ChooseImage"),
            AllowMultiple = false,
            FileTypeFilter =
            [
                new FilePickerFileType("Images")
                {
                    Patterns = ["*.gif", "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.webp"]
                }
            ]
        });

        if (files.Count == 0)
            return;

        try
        {
            await using var stream = await files[0].OpenReadAsync();
            if (stream.CanSeek && stream.Length > AnimationImageService.MaxSourceBytes)
            {
                AnimationImageStatusText = "A imagem tem mais de 16 MB.";
                return;
            }

            using var memory = new MemoryStream();
            await stream.CopyToAsync(memory, _cts.Token);
            if (memory.Length > AnimationImageService.MaxSourceBytes)
            {
                AnimationImageStatusText = "A imagem tem mais de 16 MB.";
                return;
            }
            _animationSourceBytes = memory.ToArray();
            AnimationCropZoom = 1.0;
            AnimationCropOffsetX = 0.0;
            AnimationCropOffsetY = 0.0;
            UpdateAnimationPreview();
            AnimationImagePayload prepared = PrepareAnimationImagePayload();

            CanUploadAnimationImage = true;
            AnimationSelectedFileText = files[0].Name;
            UpdateAnimationDetailsText(prepared);
            AnimationImageStatusText = "Imagem pronta para envio.";
            UpdateThemePanel(_activeThemeKey);
        }
        catch (Exception ex)
        {
            _animationSourceBytes = null;
            _animationImagePayload = null;
            CanUploadAnimationImage = false;
            AnimationImageStatusText = $"Não foi possível preparar a imagem: {ex.Message}";
            UpdateThemePanel(_activeThemeKey);
        }
    }

    [RelayCommand]
    private async Task UploadAnimationImageAsync()
    {
        if (_animationSourceBytes is null)
        {
            AnimationImageStatusText = "Escolha uma imagem primeiro.";
            return;
        }

        IsUploadingAnimationImage = true;
        CanUploadAnimationImage = false;
        AnimationImageStatusText = "Enviando imagem para o TinyDash...";

        bool uploaded = false;
        try
        {
            AnimationImagePayload prepared = PrepareAnimationImagePayload();
            UpdateAnimationDetailsText(prepared);
            using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(80));
            using var linkedCts = CancellationTokenSource.CreateLinkedTokenSource(timeoutCts.Token, _cts.Token);
            uploaded = await _deviceControlClient.UploadAnimationImageAsync(State.DisplayIp, prepared.Payload, linkedCts.Token);
        }
        catch (Exception ex)
        {
            AnimationImageStatusText = $"Nao foi possivel preparar a imagem: {ex.Message}";
            IsUploadingAnimationImage = false;
            CanUploadAnimationImage = _animationSourceBytes is not null;
            return;
        }

        IsUploadingAnimationImage = false;
        CanUploadAnimationImage = _animationSourceBytes is not null;

        if (uploaded)
        {
            AnimationImageStatusText = "Imagem enviada. O tema Animation vai renderizar direto da flash do ESP.";
            _animationImageUploaded = true;
            ThemePanelStatusText = "Imagem gravada no ESP";
            _ = Task.Run(() => RefreshDeviceStatusForDashboardAsync(_cts.Token));
        }
        else
        {
            AnimationImageStatusText = string.IsNullOrWhiteSpace(_deviceControlClient.LastError)
                ? "Falha no upload."
                : _deviceControlClient.LastError;
        }
    }

    [RelayCommand]
    private void CloseGameNames()
    {
        IsGameNamesDialogOpen = false;
    }

    [RelayCommand]
    private void CenterAnimationCrop()
    {
        AnimationCropZoom = 1.0;
        AnimationCropOffsetX = 0.0;
        AnimationCropOffsetY = 0.0;
        UpdateAnimationFramingPreview();
    }

    [RelayCommand]
    private void SaveGameAlias()
    {
        if (string.IsNullOrWhiteSpace(GameAliasProcessText) || GameAliasProcessText == "--")
        {
            GameAliasStatusText = "Nenhum processo detectado para salvar.";
            return;
        }

        if (string.IsNullOrWhiteSpace(GameAliasDisplayNameText))
        {
            GameAliasStatusText = "Digite um nome exibido primeiro.";
            return;
        }

        _gameAliasService.SaveAlias(GameAliasProcessText, GameAliasDisplayNameText);
        GamerGameText = _gameAliasService.Resolve(GameAliasProcessText);
        GameAliasStatusText = $"Salvo: {GameAliasProcessText}.";
        UpdateThemePanel(_activeThemeKey);
    }

    [RelayCommand]
    private void DeleteGameAlias()
    {
        if (string.IsNullOrWhiteSpace(GameAliasProcessText) || GameAliasProcessText == "--")
        {
            GameAliasStatusText = "Nenhum processo detectado para remover.";
            return;
        }

        _gameAliasService.DeleteAlias(GameAliasProcessText);
        GameAliasDisplayNameText = string.Empty;
        GamerGameText = GameAliasProcessText;
        GameAliasStatusText = $"Alias removido para {GameAliasProcessText}.";
        UpdateThemePanel(_activeThemeKey);
    }

    public void Dispose()
    {
        _cts.Cancel();
        _cts.Dispose();

        _metricsService.Dispose();
        _displayHttpClient.Dispose();
        _deviceControlClient.Dispose();
        _hardwareInfoService.Dispose();
        _animationSourceBytes = null;
        DisposeAnimationPreviewFrames();
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
        string normalizedTheme = NormalizeThemeKey(themeKey);
        IsGamerThemeActive = normalizedTheme == "gamer";
        IsAnimationThemeActive = normalizedTheme == "animation";

        switch (normalizedTheme)
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
                    $"Image         {(_animationImagePayload is null ? "Not selected" : "Ready")}{Environment.NewLine}" +
                    $"Format        RGB565 240x240{Environment.NewLine}" +
                    $"Upload        up to {AnimationImageService.MaxPayloadBytes / 1024} KB";
                ThemePanelStatusText = _animationImageUploaded
                    ? "Imagem gravada no ESP"
                    : "Conversão e envio serão feitos pelo Agent";
                break;

            default:
                ThemePanelTitle = "PC MONITOR";
                ThemePanelBodyText =
                    $"Ponte de telemetria ativa{Environment.NewLine}" +
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

    private static string FormatTemperatureAside(double? temperature)
    {
        return temperature.HasValue
            ? $"| {temperature.Value:0}°C"
            : string.Empty;
    }

    private static bool ShouldSendDiskMetrics(string? theme)
    {
        string normalizedTheme = NormalizeThemeKey(theme);
        return normalizedTheme == "pc_monitor" || normalizedTheme == "gamer";
    }

    private static bool ShouldSendMetricsForTheme(string? theme)
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

    private static string S(string name) => Strings.Get(name);

    private AnimationImageFraming GetAnimationFraming()
    {
        return new AnimationImageFraming(AnimationCropZoom, AnimationCropOffsetX, AnimationCropOffsetY);
    }

    private AnimationImagePayload PrepareAnimationImagePayload()
    {
        if (_animationSourceBytes is null)
            throw new InvalidOperationException("Escolha uma imagem primeiro.");

        AnimationImagePayload prepared = _animationImageService.CreateUploadPayload(_animationSourceBytes, GetAnimationFraming());
        _animationImagePayload = prepared.Payload;
        return prepared;
    }

    private void UpdateAnimationFramingPreview()
    {
        if (_animationSourceBytes is null)
            return;

        UpdateAnimationPreview();
        AnimationImageStatusText = "Enquadramento ajustado. Pronto para enviar.";
    }

    private void UpdateAnimationPreview()
    {
        if (_animationSourceBytes is null)
        {
            DisposeAnimationPreviewFrames();
            return;
        }

        try
        {
            AnimationImagePreview preview = _animationImageService.CreatePreview(_animationSourceBytes, GetAnimationFraming());
            List<Bitmap> frames = preview.Frames
                .Select(frameBytes => new Bitmap(new MemoryStream(frameBytes)))
                .ToList();

            DisposeAnimationPreviewFrames();
            _animationPreviewFrames.AddRange(frames);
            _animationPreviewFrameIndex = 0;
            AnimationPreviewImage = _animationPreviewFrames[0];
            HasAnimationPreviewImage = true;
            StartAnimationPreviewTimer(preview.DelayMs);
        }
        catch
        {
            DisposeAnimationPreviewFrames();
        }
    }

    private void StartAnimationPreviewTimer(int delayMs = 150)
    {
        StopAnimationPreviewTimer();
        if (_animationPreviewFrames.Count <= 1 || !IsAnimationImageDialogOpen)
            return;

        _animationPreviewTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(Math.Clamp(delayMs, 100, 1000))
        };
        _animationPreviewTimer.Tick += (_, _) =>
        {
            if (_animationPreviewFrames.Count == 0)
                return;

            _animationPreviewFrameIndex = (_animationPreviewFrameIndex + 1) % _animationPreviewFrames.Count;
            AnimationPreviewImage = _animationPreviewFrames[_animationPreviewFrameIndex];
        };
        _animationPreviewTimer.Start();
    }

    private void StopAnimationPreviewTimer()
    {
        _animationPreviewTimer?.Stop();
        _animationPreviewTimer = null;
    }

    private void DisposeAnimationPreviewFrames()
    {
        StopAnimationPreviewTimer();
        AnimationPreviewImage = null;
        foreach (Bitmap frame in _animationPreviewFrames)
            frame.Dispose();
        _animationPreviewFrames.Clear();
        _animationPreviewFrameIndex = 0;
        HasAnimationPreviewImage = false;
    }

    private void UpdateAnimationDetailsText(AnimationImagePayload prepared)
    {
        string frameText = prepared.FrameCount == 1 ? "1 frame estatico" : $"{prepared.FrameCount} frames, {prepared.DelayMs} ms/frame";
        AnimationImageDetailsText = $"Preparada 240x240 RGB565 - {frameText} - {prepared.Payload.Length:N0} bytes de upload";
    }

    private static Bitmap? TryCreatePreviewBitmap(byte[] sourceBytes)
    {
        try
        {
            return new Bitmap(new MemoryStream(sourceBytes));
        }
        catch
        {
            return null;
        }
    }

    private static Avalonia.Controls.TopLevel? GetMainTopLevel()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            return desktop.MainWindow;

        return null;
    }
}
