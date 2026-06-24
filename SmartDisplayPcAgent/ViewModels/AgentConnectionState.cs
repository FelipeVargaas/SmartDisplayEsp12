using CommunityToolkit.Mvvm.ComponentModel;

namespace SmartDisplayPcAgent.ViewModels;

public partial class AgentConnectionState : ObservableObject
{
    [ObservableProperty]
    private string displayIp = "192.168.0.181";

    [ObservableProperty]
    private bool sendToDisplayEnabled;

    [ObservableProperty]
    private string displayStatusText = "Envio para display desativado";

    [ObservableProperty]
    private string displayStatusShortText = "Paused";

    [ObservableProperty]
    private string lastPostText = "Disabled";
}
