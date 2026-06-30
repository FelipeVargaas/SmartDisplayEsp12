using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using SmartDisplayPcAgent.Resources;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;

namespace SmartDisplayPcAgent.ViewModels;

public partial class MainWindowViewModel : ObservableObject, IDisposable
{
    private readonly LocalizationSettingsService _localizationSettingsService = new();

    public MainWindowViewModel()
    {
        State = new AgentConnectionState();
        Dashboard = new DashboardViewModel(State);
        Device = new DeviceViewModel(State);
        AvailableLanguages =
        [
            new LanguageOption("pt-BR", Strings.Get("LanguagePortugueseBrazil")),
            new LanguageOption("en", Strings.Get("LanguageEnglish")),
        ];

        string cultureName = _localizationSettingsService.LoadCultureName();
        selectedLanguage = AvailableLanguages.FirstOrDefault(language => language.CultureName == cultureName)
            ?? AvailableLanguages[0];
    }

    public AgentConnectionState State { get; }

    public DashboardViewModel Dashboard { get; }

    public DeviceViewModel Device { get; }

    public IReadOnlyList<LanguageOption> AvailableLanguages { get; }

    [ObservableProperty]
    private LanguageOption? selectedLanguage;

    [ObservableProperty]
    private string languageRestartText = string.Empty;

    [ObservableProperty]
    private bool isLanguageRestartDialogOpen;

    [ObservableProperty]
    private string languageRestartDialogMessage = Strings.LanguageRestartDialogMessage;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsDashboardSelected))]
    [NotifyPropertyChangedFor(nameof(IsDeviceSelected))]
    private int selectedPageIndex;

    public bool IsDashboardSelected => SelectedPageIndex == 0;

    public bool IsDeviceSelected => SelectedPageIndex == 1;

    [RelayCommand]
    private void SelectDashboard()
    {
        SelectedPageIndex = 0;
    }

    [RelayCommand]
    private void SelectDevice()
    {
        SelectedPageIndex = 1;
    }

    partial void OnSelectedLanguageChanged(LanguageOption? value)
    {
        if (value is null)
            return;

        _localizationSettingsService.SaveCultureName(value.CultureName);

        string currentCultureName = LocalizationSettingsService.NormalizeCultureName(CultureInfo.CurrentUICulture.Name);
        LanguageRestartText = value.CultureName == currentCultureName
            ? string.Empty
            : Strings.LanguageRestartHint;

        IsLanguageRestartDialogOpen = !string.IsNullOrWhiteSpace(LanguageRestartText);
        LanguageRestartDialogMessage = Strings.LanguageRestartDialogMessage;
    }

    [RelayCommand]
    private void RestartLater()
    {
        IsLanguageRestartDialogOpen = false;
    }

    [RelayCommand]
    private void RestartNow()
    {
        try
        {
            string? executablePath = Environment.ProcessPath;

            if (string.IsNullOrWhiteSpace(executablePath))
                executablePath = Process.GetCurrentProcess().MainModule?.FileName;

            if (string.IsNullOrWhiteSpace(executablePath))
                throw new InvalidOperationException(Strings.RestartPathUnavailable);

            Process.Start(new ProcessStartInfo(executablePath)
            {
                UseShellExecute = true,
            });

            if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
                desktop.Shutdown();
        }
        catch (Exception ex)
        {
            LanguageRestartDialogMessage = string.Format(Strings.RestartFailedFormat, ex.Message);
        }
    }

    public void Dispose()
    {
        Dashboard.Dispose();
        Device.Dispose();
    }
}
