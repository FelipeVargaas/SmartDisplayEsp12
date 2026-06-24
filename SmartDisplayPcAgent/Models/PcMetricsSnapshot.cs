using System.Collections.Generic;

namespace SmartDisplayPcAgent.Models;

public sealed record PcMetricsSnapshot(
    double CpuUsage,
    double RamUsage,
    double GpuUsage,
    double DiskUsage = 0,
    string DiskLabel = "---",
    IReadOnlyList<DiskMetricsSnapshot>? Disks = null,
    double? GpuTemperature = null,
    int? Fps = null,
    double? Frametime = null,
    string Game = "",
    string? Source = null
);
