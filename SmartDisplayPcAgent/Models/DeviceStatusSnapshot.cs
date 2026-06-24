using System;

namespace SmartDisplayPcAgent.Models;

public sealed record DeviceStatusSnapshot(
    string Name,
    string Mode,
    string Ip,
    string Ssid,
    int? Rssi,
    string Theme,
    double Cpu,
    double Ram,
    double Gpu,
    double Disk,
    string DiskLabel,
    bool PcOnline,
    long? LastPcMetricsAgeMs,
    double? Temperature,
    string Weather,
    long? Heap,
    long? FlashSize)
{
    public static DeviceStatusSnapshot Empty { get; } = new(
        Name: "TinyDash",
        Mode: "--",
        Ip: "--",
        Ssid: "--",
        Rssi: null,
        Theme: "pc_monitor",
        Cpu: 0,
        Ram: 0,
        Gpu: 0,
        Disk: 0,
        DiskLabel: "---",
        PcOnline: false,
        LastPcMetricsAgeMs: null,
        Temperature: null,
        Weather: "--",
        Heap: null,
        FlashSize: null);
}
