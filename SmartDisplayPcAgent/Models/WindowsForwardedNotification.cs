namespace SmartDisplayPcAgent.Models;

public sealed record WindowsForwardedNotification(
    string AppName,
    string Sender,
    string Title,
    string Time,
    string Accent,
    int DurationMs);
