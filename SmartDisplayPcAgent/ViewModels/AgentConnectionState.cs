using CommunityToolkit.Mvvm.ComponentModel;

namespace SmartDisplayPcAgent.ViewModels;

public partial class AgentConnectionState : ObservableObject
{
    [ObservableProperty]
    private string displayIp = "192.168.0.181";

    [ObservableProperty]
    private bool sendToDisplayEnabled = true;

    [ObservableProperty]
    private string displayStatusText = "Pronto para enviar telemetria";

    [ObservableProperty]
    private string displayStatusShortText = "Ready";

    [ObservableProperty]
    private string lastPostText = "Waiting";
}
