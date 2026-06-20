namespace SmartDisplayPcAgent.Models;

public sealed record PcMetricsSnapshot(
    double CpuUsage,
    double RamUsage,
    double GpuUsage
);