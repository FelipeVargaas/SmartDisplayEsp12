using CommunityToolkit.Mvvm.ComponentModel;
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

    public void Dispose()
    {
        Dashboard.Dispose();
        Device.Dispose();
    }
}
