using CommunityToolkit.Mvvm.ComponentModel;
using SmartDisplayPcAgent.Resources;

namespace SmartDisplayPcAgent.ViewModels;

public partial class AgentConnectionState : ObservableObject
{
    [ObservableProperty]
    private string displayIp = "192.168.0.181";

    [ObservableProperty]
    private bool sendToDisplayEnabled = true;

    [ObservableProperty]
    private string activeThemeKey = "pc_monitor";

    [ObservableProperty]
    private string displayStatusText = Strings.Get("StatusReadyToSend");

    [ObservableProperty]
    private string displayStatusShortText = Strings.Get("StatusReady");

    [ObservableProperty]
    private string lastPostText = Strings.Get("StatusWaiting");
}
