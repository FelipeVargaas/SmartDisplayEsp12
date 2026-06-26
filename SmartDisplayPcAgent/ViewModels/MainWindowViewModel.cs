using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using System;

namespace SmartDisplayPcAgent.ViewModels;

public partial class MainWindowViewModel : ObservableObject, IDisposable
{
    public MainWindowViewModel()
    {
        State = new AgentConnectionState();
        Dashboard = new DashboardViewModel(State);
        Device = new DeviceViewModel(State);
    }

    public AgentConnectionState State { get; }

    public DashboardViewModel Dashboard { get; }

    public DeviceViewModel Device { get; }

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

    public void Dispose()
    {
        Dashboard.Dispose();
        Device.Dispose();
    }
}
