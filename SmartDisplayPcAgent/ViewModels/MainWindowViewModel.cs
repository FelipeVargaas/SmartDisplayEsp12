using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SmartDisplayPcAgent.Resources;
using SmartDisplayPcAgent.Services;
using System;
using System.Collections.Generic;
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
    }

    public void Dispose()
    {
        Dashboard.Dispose();
        Device.Dispose();
    }
}
