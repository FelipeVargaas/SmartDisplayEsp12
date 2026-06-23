using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using LibreHardwareMonitor.Hardware;
using SmartDisplayPcAgent.Models;

namespace SmartDisplayPcAgent.Services;

public sealed class PcMetricsService : IDisposable
{
    private readonly PerformanceCounter _cpuCounter;
    private readonly List<DiskCounterSet> _diskCounters = [];

    private Computer? _computer;
    private bool _hardwareMonitorAvailable;

    private DateTime _lastDiskRefresh = DateTime.MinValue;

    public PcMetricsService()
    {
        _cpuCounter = new PerformanceCounter("Processor", "% Processor Time", "_Total", readOnly: true);

        // Primeira leitura costuma vir zerada.
        _cpuCounter.NextValue();

        InitializeHardwareMonitor();
        RefreshDiskCounters();
    }

    public PcMetricsSnapshot GetSnapshot()
    {
        double cpu = GetCpuUsagePercent();
        double ram = GetRamUsagePercent();
        double gpu = GetGpuUsagePercent();

        RefreshDiskCountersIfNeeded();

        var disks = GetDiskMetrics();
        var selectedDisk = SelectDisplayDisk(disks);

        return new PcMetricsSnapshot(
            cpu,
            ram,
            gpu,
            selectedDisk?.ActiveUsage ?? 0,
            selectedDisk?.Label ?? "---",
            disks);
    }

    private double GetCpuUsagePercent()
    {
        try
        {
            return ClampPercent(_cpuCounter.NextValue());
        }
        catch
        {
            return 0;
        }
    }

    private static double GetRamUsagePercent()
    {
        try
        {
            var memoryStatus = new MemoryStatusEx();

            if (!GlobalMemoryStatusEx(memoryStatus))
                return 0;

            return ClampPercent(memoryStatus.dwMemoryLoad);
        }
        catch
        {
            return 0;
        }
    }

    private void InitializeHardwareMonitor()
    {
        try
        {
            _computer = new Computer
            {
                IsGpuEnabled = true
            };

            _computer.Open();
            _hardwareMonitorAvailable = true;
        }
        catch
        {
            _hardwareMonitorAvailable = false;

            try
            {
                _computer?.Close();
            }
            catch
            {
                // Ignora.
            }

            _computer = null;
        }
    }

    private double GetGpuUsagePercent()
    {
        if (!_hardwareMonitorAvailable || _computer is null)
            return 0;

        try
        {
            var gpuHardware = _computer.Hardware
                .Where(h =>
                    h.HardwareType == HardwareType.GpuAmd ||
                    h.HardwareType == HardwareType.GpuNvidia ||
                    h.HardwareType == HardwareType.GpuIntel)
                .ToList();

            if (gpuHardware.Count == 0)
                return 0;

            var fallbackValues = new List<double>();

            foreach (var hardware in gpuHardware)
            {
                UpdateHardwareRecursive(hardware);

                foreach (var sensor in GetSensorsRecursive(hardware))
                {
                    if (sensor.SensorType != SensorType.Load)
                        continue;

                    if (!sensor.Value.HasValue)
                        continue;

                    string name = sensor.Name;

                    if (name.Equals("GPU Core", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("GPU Total", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("GPU 3D", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("D3D 3D", StringComparison.OrdinalIgnoreCase))
                    {
                        return ClampPercent(sensor.Value.Value);
                    }

                    if (name.Contains("Memory", StringComparison.OrdinalIgnoreCase) ||
                        name.Contains("Bus", StringComparison.OrdinalIgnoreCase) ||
                        name.Contains("Copy", StringComparison.OrdinalIgnoreCase) ||
                        name.Contains("Video", StringComparison.OrdinalIgnoreCase) ||
                        name.Contains("Encode", StringComparison.OrdinalIgnoreCase) ||
                        name.Contains("Decode", StringComparison.OrdinalIgnoreCase))
                    {
                        continue;
                    }

                    fallbackValues.Add(sensor.Value.Value);
                }
            }

            if (fallbackValues.Count == 0)
                return 0;

            return ClampPercent(fallbackValues.Average());
        }
        catch
        {
            return 0;
        }
    }

    private void RefreshDiskCountersIfNeeded()
    {
        if ((DateTime.Now - _lastDiskRefresh).TotalSeconds < 30)
            return;

        RefreshDiskCounters();
    }

    private void RefreshDiskCounters()
    {
        _lastDiskRefresh = DateTime.Now;

        DisposeDiskCounters();

        try
        {
            if (!PerformanceCounterCategory.Exists("LogicalDisk"))
                return;

            string systemDrive = GetSystemDriveLetter();
            int hdIndex = 0;

            var drives = DriveInfo.GetDrives()
                .Where(d => d.DriveType == DriveType.Fixed && d.IsReady)
                .OrderBy(d => d.Name)
                .ToList();

            foreach (var drive in drives)
            {
                string driveLetter = drive.Name.TrimEnd('\\');

                if (string.IsNullOrWhiteSpace(driveLetter))
                    continue;

                bool isSystemDisk = driveLetter.Equals(systemDrive, StringComparison.OrdinalIgnoreCase);

                string label = isSystemDisk
                    ? "SSD"
                    : $"HD{hdIndex++}";

                try
                {
                    var activeCounter = new PerformanceCounter(
                        "LogicalDisk",
                        "% Disk Time",
                        driveLetter,
                        readOnly: true);

                    var readCounter = new PerformanceCounter(
                        "LogicalDisk",
                        "Disk Read Bytes/sec",
                        driveLetter,
                        readOnly: true);

                    var writeCounter = new PerformanceCounter(
                        "LogicalDisk",
                        "Disk Write Bytes/sec",
                        driveLetter,
                        readOnly: true);

                    // Aquecimento dos contadores.
                    activeCounter.NextValue();
                    readCounter.NextValue();
                    writeCounter.NextValue();

                    _diskCounters.Add(new DiskCounterSet(
                        label,
                        driveLetter,
                        isSystemDisk,
                        activeCounter,
                        readCounter,
                        writeCounter));
                }
                catch
                {
                    // Ignora disco que não tenha contador válido.
                }
            }
        }
        catch
        {
            // Sem disco por enquanto.
        }
    }

    private IReadOnlyList<DiskMetricsSnapshot> GetDiskMetrics()
    {
        var result = new List<DiskMetricsSnapshot>();

        foreach (var disk in _diskCounters.ToList())
        {
            try
            {
                double activeUsage = ClampPercent(disk.ActiveUsageCounter.NextValue());
                double readMb = BytesToMb(disk.ReadBytesCounter.NextValue());
                double writeMb = BytesToMb(disk.WriteBytesCounter.NextValue());
                double usedSpace = GetDriveUsedPercent(disk.DriveLetter);

                result.Add(new DiskMetricsSnapshot(
                    disk.Label,
                    disk.DriveLetter,
                    activeUsage,
                    usedSpace,
                    readMb,
                    writeMb,
                    disk.IsSystemDisk));
            }
            catch
            {
                // Contador pode morrer se disco/removível mudar.
            }
        }

        return result;
    }

    private static DiskMetricsSnapshot? SelectDisplayDisk(IReadOnlyList<DiskMetricsSnapshot> disks)
    {
        if (disks.Count == 0)
            return null;

        // Por enquanto: disco principal.
        // Depois trocamos para selecionado ou mais ativo.
        return disks.FirstOrDefault(d => d.IsSystemDisk) ?? disks.FirstOrDefault();
    }

    private static double GetDriveUsedPercent(string driveLetter)
    {
        try
        {
            var drive = new DriveInfo(driveLetter + "\\");

            if (!drive.IsReady || drive.TotalSize <= 0)
                return 0;

            double used = drive.TotalSize - drive.AvailableFreeSpace;

            return ClampPercent((used / drive.TotalSize) * 100.0);
        }
        catch
        {
            return 0;
        }
    }

    private static string GetSystemDriveLetter()
    {
        try
        {
            string? root = Path.GetPathRoot(Environment.SystemDirectory);

            if (string.IsNullOrWhiteSpace(root))
                return "C:";

            return root.TrimEnd('\\');
        }
        catch
        {
            return "C:";
        }
    }

    private static double BytesToMb(float bytes)
    {
        if (float.IsNaN(bytes) || float.IsInfinity(bytes) || bytes < 0)
            return 0;

        return bytes / 1024.0 / 1024.0;
    }

    private static void UpdateHardwareRecursive(IHardware hardware)
    {
        hardware.Update();

        foreach (var subHardware in hardware.SubHardware)
        {
            UpdateHardwareRecursive(subHardware);
        }
    }

    private static IEnumerable<ISensor> GetSensorsRecursive(IHardware hardware)
    {
        foreach (var sensor in hardware.Sensors)
        {
            yield return sensor;
        }

        foreach (var subHardware in hardware.SubHardware)
        {
            foreach (var sensor in GetSensorsRecursive(subHardware))
            {
                yield return sensor;
            }
        }
    }

    private static double ClampPercent(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value))
            return 0;

        return Math.Clamp(value, 0, 100);
    }

    private void DisposeDiskCounters()
    {
        foreach (var disk in _diskCounters)
        {
            try
            {
                disk.Dispose();
            }
            catch
            {
                // Ignora.
            }
        }

        _diskCounters.Clear();
    }

    public void Dispose()
    {
        try
        {
            _cpuCounter.Dispose();
        }
        catch
        {
            // Ignora.
        }

        try
        {
            _computer?.Close();
        }
        catch
        {
            // Ignora.
        }

        DisposeDiskCounters();
    }

    private sealed class DiskCounterSet : IDisposable
    {
        public DiskCounterSet(
            string label,
            string driveLetter,
            bool isSystemDisk,
            PerformanceCounter activeUsageCounter,
            PerformanceCounter readBytesCounter,
            PerformanceCounter writeBytesCounter)
        {
            Label = label;
            DriveLetter = driveLetter;
            IsSystemDisk = isSystemDisk;
            ActiveUsageCounter = activeUsageCounter;
            ReadBytesCounter = readBytesCounter;
            WriteBytesCounter = writeBytesCounter;
        }

        public string Label { get; }
        public string DriveLetter { get; }
        public bool IsSystemDisk { get; }

        public PerformanceCounter ActiveUsageCounter { get; }
        public PerformanceCounter ReadBytesCounter { get; }
        public PerformanceCounter WriteBytesCounter { get; }

        public void Dispose()
        {
            ActiveUsageCounter.Dispose();
            ReadBytesCounter.Dispose();
            WriteBytesCounter.Dispose();
        }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatusEx lpBuffer);

    [StructLayout(LayoutKind.Sequential)]
    private sealed class MemoryStatusEx
    {
        public uint dwLength = (uint)Marshal.SizeOf<MemoryStatusEx>();
        public uint dwMemoryLoad;
        public ulong ullTotalPhys;
        public ulong ullAvailPhys;
        public ulong ullTotalPageFile;
        public ulong ullAvailPageFile;
        public ulong ullTotalVirtual;
        public ulong ullAvailVirtual;
        public ulong ullAvailExtendedVirtual;
    }
}