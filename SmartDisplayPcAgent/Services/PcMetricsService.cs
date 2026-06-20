using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using LibreHardwareMonitor.Hardware;
using SmartDisplayPcAgent.Models;

namespace SmartDisplayPcAgent.Services;

public sealed class PcMetricsService : IDisposable
{
    private readonly PerformanceCounter _cpuCounter;

    private Computer? _computer;
    private bool _hardwareMonitorAvailable;

    public PcMetricsService()
    {
        _cpuCounter = new PerformanceCounter("Processor", "% Processor Time", "_Total", readOnly: true);

        // Primeira leitura costuma vir zerada.
        _cpuCounter.NextValue();

        InitializeHardwareMonitor();
    }

    public PcMetricsSnapshot GetSnapshot()
    {
        double cpu = GetCpuUsagePercent();
        double ram = GetRamUsagePercent();
        double gpu = GetGpuUsagePercent();

        return new PcMetricsSnapshot(cpu, ram, gpu);
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

                    // Sensor principal mais provável.
                    if (name.Equals("GPU Core", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("GPU Total", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("GPU 3D", StringComparison.OrdinalIgnoreCase) ||
                        name.Equals("D3D 3D", StringComparison.OrdinalIgnoreCase))
                    {
                        return ClampPercent(sensor.Value.Value);
                    }

                    // Ignora sensores que podem distorcer a leitura principal.
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

            // Melhor que Max(): evita um sensor isolado jogar tudo para 100%.
            return ClampPercent(fallbackValues.Average());
        }
        catch
        {
            return 0;
        }
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