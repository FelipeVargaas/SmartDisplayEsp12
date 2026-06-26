namespace SmartDisplayPcAgent.Models;

public sealed record GamerTelemetrySnapshot(
    string Game = "",
    int? Fps = null,
    double? Frametime = null,
    string? Source = null,
    string Status = "RTSS OFF")
{
    public bool HasValidData => Fps.HasValue || Frametime.HasValue;
}
