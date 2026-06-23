namespace SmartDisplayPcAgent.Models;

public sealed record DiskMetricsSnapshot(
    string Label,
    string DriveLetter,
    double ActiveUsage,
    double UsedSpace,
    double ReadMbPerSecond,
    double WriteMbPerSecond,
    bool IsSystemDisk
);