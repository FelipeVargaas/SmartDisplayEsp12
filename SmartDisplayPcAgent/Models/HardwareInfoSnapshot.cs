namespace SmartDisplayPcAgent.Models;

public sealed record HardwareInfoSnapshot(
    string CpuName,
    string CpuCoresText,
    string CpuThreadsText,
    string CpuClockText,
    string MemoryTotalText,
    string MemorySpeedText,
    string MemoryModulesText,
    string MemoryChannelText,
    string GpuName,
    string GpuVramText,
    string GpuClockText,
    string StorageName,
    string StorageTotalText,
    string StorageUsedText,
    double StorageUsedPercent
);
