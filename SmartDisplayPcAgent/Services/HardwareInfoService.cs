using LibreHardwareMonitor.Hardware;
using Microsoft.Win32;
using SmartDisplayPcAgent.Models;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace SmartDisplayPcAgent.Services;

public sealed class HardwareInfoService : IDisposable
{
    private Computer? _computer;

    public HardwareInfoService()
    {
        try
        {
            _computer = new Computer
            {
                IsGpuEnabled = true
            };

            _computer.Open();
        }
        catch
        {
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

    public HardwareInfoSnapshot GetSnapshot()
    {
        var processorInfo = GetProcessorInfo();
        var memoryInfo = GetMemoryInfo();
        var gpuInfo = GetGpuInfo();
        var storageInfo = GetPrimaryStorageInfo();

        return new HardwareInfoSnapshot(
            CpuName: processorInfo.Name,
            CpuCoresText: processorInfo.CoresText,
            CpuThreadsText: processorInfo.ThreadsText,
            CpuClockText: processorInfo.ClockText,
            MemoryTotalText: memoryInfo.TotalText,
            MemorySpeedText: memoryInfo.SpeedText,
            MemoryModulesText: memoryInfo.ModulesText,
            MemoryChannelText: memoryInfo.ChannelText,
            GpuName: gpuInfo.Name,
            GpuVramText: gpuInfo.VramText,
            GpuClockText: gpuInfo.ClockText,
            StorageName: storageInfo.Name,
            StorageTotalText: storageInfo.TotalText,
            StorageUsedText: storageInfo.UsedText,
            StorageUsedPercent: storageInfo.UsedPercent);
    }

    private static ProcessorInfo GetProcessorInfo()
    {
        string name = ReadRegistryString(
            @"HARDWARE\DESCRIPTION\System\CentralProcessor\0",
            "ProcessorNameString") ?? "Unknown CPU";

        int threads = Environment.ProcessorCount;
        int cores = TryGetPhysicalCoreCount();

        if (cores <= 0 || cores > threads)
            cores = threads;

        string clockText = "--";
        object? mhzValue = ReadRegistryValue(
            @"HARDWARE\DESCRIPTION\System\CentralProcessor\0",
            "~MHz");

        if (mhzValue is int mhz && mhz > 0)
            clockText = FormatClockMhz(mhz);

        return new ProcessorInfo(
            CleanCpuName(name),
            cores.ToString(CultureInfo.InvariantCulture),
            threads.ToString(CultureInfo.InvariantCulture),
            clockText);
    }

    private static MemoryInfo GetMemoryInfo()
    {
        string totalText = GetTotalMemoryText();

        string speedText = "--";
        string modulesText = "--";
        string channelText = "Unknown";

        var modules = GetMemoryModulesFromPowerShell();

        if (modules.Count > 0)
        {
            modulesText = modules.Count.ToString(CultureInfo.InvariantCulture);

            var speeds = modules
                .Select(m => m.SpeedMhz)
                .Where(v => v > 0)
                .Distinct()
                .OrderBy(v => v)
                .ToList();

            if (speeds.Count == 1)
                speedText = $"{speeds[0]:0} MHz";
            else if (speeds.Count > 1)
                speedText = string.Join(" / ", speeds.Select(v => $"{v:0} MHz"));
        }

        return new MemoryInfo(totalText, speedText, modulesText, channelText);
    }

    private GpuInfo GetGpuInfo()
    {
        string name = "Unknown GPU";
        string vramText = "--";
        string clockText = "--";

        try
        {
            var gpuHardware = _computer?.Hardware
                .Where(IsGpuHardware)
                .ToList() ?? [];

            var selectedGpu = gpuHardware.FirstOrDefault(h =>
                h.HardwareType == HardwareType.GpuNvidia ||
                h.HardwareType == HardwareType.GpuAmd) ?? gpuHardware.FirstOrDefault();

            if (selectedGpu is not null)
            {
                name = CleanGpuName(selectedGpu.Name);
                UpdateHardwareRecursive(selectedGpu);

                foreach (var sensor in GetSensorsRecursive(selectedGpu))
                {
                    if (sensor.SensorType != SensorType.Clock || !sensor.Value.HasValue)
                        continue;

                    if (sensor.Name.Equals("GPU Core", StringComparison.OrdinalIgnoreCase) ||
                        sensor.Name.Contains("Core", StringComparison.OrdinalIgnoreCase))
                    {
                        clockText = $"{sensor.Value.Value:0} MHz";
                        break;
                    }
                }
            }
        }
        catch
        {
            // Mantém fallback.
        }

        // DXGI costuma retornar VRAM dedicada corretamente em 64 bits.
        // Win32_VideoController.AdapterRAM frequentemente trunca placas acima de 4 GB.
        var dxgiGpu = GetPrimaryGpuFromDxgi(name);

        if (dxgiGpu is not null)
        {
            name = CleanGpuName(dxgiGpu.Name);

            if (dxgiGpu.DedicatedVideoMemoryBytes > 0)
                vramText = FormatBytes(dxgiGpu.DedicatedVideoMemoryBytes, preferGb: true);
        }
        else
        {
            string? cimGpu = GetPrimaryGpuFromPowerShell(out ulong adapterRamBytes);

            if (!string.IsNullOrWhiteSpace(cimGpu))
                name = CleanGpuName(cimGpu);

            if (adapterRamBytes > 0)
                vramText = FormatBytes(adapterRamBytes, preferGb: true);
        }

        return new GpuInfo(name, vramText, clockText);
    }

    private static DxgiGpuInfo? GetPrimaryGpuFromDxgi(string preferredName)
    {
        IntPtr factory = IntPtr.Zero;

        try
        {
            Guid factoryGuid = new("770aae78-f26f-4dba-a829-253c83d1b387"); // IDXGIFactory1
            int hr = CreateDXGIFactory1(ref factoryGuid, out factory);

            if (hr < 0 || factory == IntPtr.Zero)
                return null;

            var adapters = new List<DxgiGpuInfo>();
            IntPtr factoryVtable = Marshal.ReadIntPtr(factory);
            IntPtr enumAdapters1Ptr = Marshal.ReadIntPtr(factoryVtable, IntPtr.Size * 12);
            var enumAdapters1 = Marshal.GetDelegateForFunctionPointer<EnumAdapters1Delegate>(enumAdapters1Ptr);

            for (uint index = 0; index < 16; index++)
            {
                IntPtr adapter = IntPtr.Zero;
                int enumHr = enumAdapters1(factory, index, out adapter);

                // DXGI_ERROR_NOT_FOUND = 0x887A0002
                if (enumHr == unchecked((int)0x887A0002))
                    break;

                if (enumHr < 0 || adapter == IntPtr.Zero)
                    continue;

                try
                {
                    var desc = GetAdapterDescription(adapter);
                    string adapterName = desc.Description?.TrimEnd('\0').Trim() ?? string.Empty;

                    if (string.IsNullOrWhiteSpace(adapterName) ||
                        adapterName.Contains("Microsoft Basic Render", StringComparison.OrdinalIgnoreCase))
                    {
                        continue;
                    }

                    ulong dedicatedBytes = desc.DedicatedVideoMemory.ToUInt64();
                    adapters.Add(new DxgiGpuInfo(adapterName, dedicatedBytes));
                }
                finally
                {
                    ReleaseComObject(adapter);
                }
            }

            if (adapters.Count == 0)
                return null;

            string preferred = CleanGpuName(preferredName);

            var matched = adapters.FirstOrDefault(a =>
                !string.IsNullOrWhiteSpace(preferred) &&
                (CleanGpuName(a.Name).Contains(preferred, StringComparison.OrdinalIgnoreCase) ||
                 preferred.Contains(CleanGpuName(a.Name), StringComparison.OrdinalIgnoreCase)));

            return matched ?? adapters
                .OrderByDescending(a => a.DedicatedVideoMemoryBytes)
                .FirstOrDefault();
        }
        catch
        {
            return null;
        }
        finally
        {
            if (factory != IntPtr.Zero)
                ReleaseComObject(factory);
        }
    }

    private static DxgiAdapterDesc1 GetAdapterDescription(IntPtr adapter)
    {
        IntPtr adapterVtable = Marshal.ReadIntPtr(adapter);
        IntPtr getDesc1Ptr = Marshal.ReadIntPtr(adapterVtable, IntPtr.Size * 10);
        var getDesc1 = Marshal.GetDelegateForFunctionPointer<GetDesc1Delegate>(getDesc1Ptr);

        int hr = getDesc1(adapter, out DxgiAdapterDesc1 desc);
        return hr < 0 ? default : desc;
    }

    private static void ReleaseComObject(IntPtr comObject)
    {
        if (comObject == IntPtr.Zero)
            return;

        IntPtr vtable = Marshal.ReadIntPtr(comObject);
        IntPtr releasePtr = Marshal.ReadIntPtr(vtable, IntPtr.Size * 2);
        var release = Marshal.GetDelegateForFunctionPointer<ComReleaseDelegate>(releasePtr);
        _ = release(comObject);
    }

    private static StorageInfo GetPrimaryStorageInfo()
    {
        try
        {
            string systemDrive = Path.GetPathRoot(Environment.SystemDirectory)?.TrimEnd('\\') ?? "C:";
            var drive = DriveInfo.GetDrives()
                .Where(d => d.DriveType == DriveType.Fixed && d.IsReady)
                .OrderByDescending(d => d.Name.TrimEnd('\\').Equals(systemDrive, StringComparison.OrdinalIgnoreCase))
                .ThenBy(d => d.Name)
                .FirstOrDefault();

            if (drive is null || drive.TotalSize <= 0)
                return new StorageInfo("Storage", "--", "--", 0);

            long total = drive.TotalSize;
            long used = drive.TotalSize - drive.AvailableFreeSpace;
            double usedPercent = Math.Clamp((used / (double)total) * 100.0, 0, 100);

            string name = string.IsNullOrWhiteSpace(drive.VolumeLabel)
                ? drive.Name.TrimEnd('\\')
                : $"{drive.Name.TrimEnd('\\')} · {drive.VolumeLabel}";

            return new StorageInfo(
                name,
                FormatBytes((ulong)total, preferGb: true),
                $"{FormatBytes((ulong)used, preferGb: true)} / {usedPercent:0}%",
                usedPercent);
        }
        catch
        {
            return new StorageInfo("Storage", "--", "--", 0);
        }
    }

    private static string GetTotalMemoryText()
    {
        try
        {
            var memoryStatus = new MemoryStatusEx();

            if (!GlobalMemoryStatusEx(memoryStatus))
                return "--";

            return FormatBytes(memoryStatus.ullTotalPhys, preferGb: true);
        }
        catch
        {
            return "--";
        }
    }

    private static List<MemoryModuleInfo> GetMemoryModulesFromPowerShell()
    {
        try
        {
            const string command = "Get-CimInstance Win32_PhysicalMemory | Select-Object Speed,ConfiguredClockSpeed,Capacity | ConvertTo-Json -Compress";
            string output = RunPowerShell(command, TimeSpan.FromSeconds(3));

            if (string.IsNullOrWhiteSpace(output))
                return [];

            using var document = JsonDocument.Parse(output);
            var result = new List<MemoryModuleInfo>();

            if (document.RootElement.ValueKind == JsonValueKind.Array)
            {
                foreach (var item in document.RootElement.EnumerateArray())
                    AddMemoryModule(result, item);
            }
            else if (document.RootElement.ValueKind == JsonValueKind.Object)
            {
                AddMemoryModule(result, document.RootElement);
            }

            return result;
        }
        catch
        {
            return [];
        }
    }

    private static void AddMemoryModule(List<MemoryModuleInfo> result, JsonElement item)
    {
        int configuredClock = GetJsonInt(item, "ConfiguredClockSpeed");
        int speed = configuredClock > 0 ? configuredClock : GetJsonInt(item, "Speed");

        if (TryGetJsonUInt64(item, "Capacity", out ulong capacity) && capacity == 0)
            capacity = 0;

        result.Add(new MemoryModuleInfo(speed, capacity));
    }

    private static string? GetPrimaryGpuFromPowerShell(out ulong adapterRamBytes)
    {
        adapterRamBytes = 0;

        try
        {
            const string command = "Get-CimInstance Win32_VideoController | Sort-Object AdapterRAM -Descending | Select-Object -First 1 Name,AdapterRAM | ConvertTo-Json -Compress";
            string output = RunPowerShell(command, TimeSpan.FromSeconds(3));

            if (string.IsNullOrWhiteSpace(output))
                return null;

            using var document = JsonDocument.Parse(output);
            var root = document.RootElement;

            string? name = root.TryGetProperty("Name", out var nameProperty)
                ? nameProperty.GetString()
                : null;

            if (TryGetJsonUInt64(root, "AdapterRAM", out ulong ram))
                adapterRamBytes = ram;

            return name;
        }
        catch
        {
            return null;
        }
    }

    private static string RunPowerShell(string command, TimeSpan timeout)
    {
        try
        {
            using var process = new Process();
            process.StartInfo.FileName = "powershell.exe";
            process.StartInfo.Arguments = $"-NoProfile -ExecutionPolicy Bypass -Command \"{command}\"";
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.StartInfo.CreateNoWindow = true;

            process.Start();

            if (!process.WaitForExit((int)timeout.TotalMilliseconds))
            {
                try
                {
                    process.Kill(entireProcessTree: true);
                }
                catch
                {
                    // Ignora.
                }

                return string.Empty;
            }

            return process.StandardOutput.ReadToEnd().Trim();
        }
        catch
        {
            return string.Empty;
        }
    }

    private static int TryGetPhysicalCoreCount()
    {
        try
        {
            int bufferLength = 0;
            _ = GetLogicalProcessorInformation(IntPtr.Zero, ref bufferLength);

            if (bufferLength <= 0)
                return 0;

            IntPtr buffer = Marshal.AllocHGlobal(bufferLength);

            try
            {
                if (!GetLogicalProcessorInformation(buffer, ref bufferLength))
                    return 0;

                int size = Marshal.SizeOf<SYSTEM_LOGICAL_PROCESSOR_INFORMATION>();
                int count = bufferLength / size;
                int cores = 0;

                for (int i = 0; i < count; i++)
                {
                    IntPtr itemPointer = IntPtr.Add(buffer, i * size);
                    var item = Marshal.PtrToStructure<SYSTEM_LOGICAL_PROCESSOR_INFORMATION>(itemPointer);

                    if (item.Relationship == LOGICAL_PROCESSOR_RELATIONSHIP.RelationProcessorCore)
                        cores++;
                }

                return cores;
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }
        catch
        {
            return 0;
        }
    }

    private static object? ReadRegistryValue(string subKey, string valueName)
    {
        try
        {
            using var key = Registry.LocalMachine.OpenSubKey(subKey);
            return key?.GetValue(valueName);
        }
        catch
        {
            return null;
        }
    }

    private static string? ReadRegistryString(string subKey, string valueName)
    {
        return ReadRegistryValue(subKey, valueName)?.ToString();
    }

    private static string FormatClockMhz(int mhz)
    {
        if (mhz >= 1000)
            return $"{mhz / 1000.0:0.0} GHz";

        return $"{mhz:0} MHz";
    }

    private static string FormatBytes(ulong bytes, bool preferGb)
    {
        double value = bytes;

        if (preferGb && value >= 1024d * 1024d * 1024d)
            return $"{value / (1024d * 1024d * 1024d):0.#} GB";

        if (value >= 1024d * 1024d * 1024d)
            return $"{value / (1024d * 1024d * 1024d):0.#} GB";

        if (value >= 1024d * 1024d)
            return $"{value / (1024d * 1024d):0.#} MB";

        if (value >= 1024d)
            return $"{value / 1024d:0.#} KB";

        return $"{value:0} B";
    }

    private static string CleanCpuName(string value)
    {
        return value
            .Replace("(R)", "", StringComparison.OrdinalIgnoreCase)
            .Replace("(TM)", "", StringComparison.OrdinalIgnoreCase)
            .Replace("CPU", "", StringComparison.OrdinalIgnoreCase)
            .Replace("  ", " ", StringComparison.OrdinalIgnoreCase)
            .Trim();
    }

    private static string CleanGpuName(string value)
    {
        return value
            .Replace("NVIDIA GeForce ", "NVIDIA ", StringComparison.OrdinalIgnoreCase)
            .Replace("AMD Radeon ", "AMD ", StringComparison.OrdinalIgnoreCase)
            .Trim();
    }

    private static bool IsGpuHardware(IHardware hardware)
    {
        return hardware.HardwareType == HardwareType.GpuAmd ||
               hardware.HardwareType == HardwareType.GpuNvidia ||
               hardware.HardwareType == HardwareType.GpuIntel;
    }

    private static void UpdateHardwareRecursive(IHardware hardware)
    {
        hardware.Update();

        foreach (var subHardware in hardware.SubHardware)
            UpdateHardwareRecursive(subHardware);
    }

    private static IEnumerable<ISensor> GetSensorsRecursive(IHardware hardware)
    {
        foreach (var sensor in hardware.Sensors)
            yield return sensor;

        foreach (var subHardware in hardware.SubHardware)
        {
            foreach (var sensor in GetSensorsRecursive(subHardware))
                yield return sensor;
        }
    }

    private static int GetJsonInt(JsonElement item, string propertyName)
    {
        if (!item.TryGetProperty(propertyName, out var property))
            return 0;

        return property.ValueKind switch
        {
            JsonValueKind.Number when property.TryGetInt32(out int value) => value,
            JsonValueKind.String when int.TryParse(property.GetString(), out int value) => value,
            _ => 0
        };
    }

    private static bool TryGetJsonUInt64(JsonElement item, string propertyName, out ulong value)
    {
        value = 0;

        if (!item.TryGetProperty(propertyName, out var property))
            return false;

        if (property.ValueKind == JsonValueKind.Number && property.TryGetUInt64(out value))
            return true;

        if (property.ValueKind == JsonValueKind.String && ulong.TryParse(property.GetString(), out value))
            return true;

        return false;
    }

    public void Dispose()
    {
        try
        {
            _computer?.Close();
        }
        catch
        {
            // Ignora.
        }
    }

    private sealed record ProcessorInfo(string Name, string CoresText, string ThreadsText, string ClockText);
    private sealed record MemoryInfo(string TotalText, string SpeedText, string ModulesText, string ChannelText);
    private sealed record MemoryModuleInfo(int SpeedMhz, ulong CapacityBytes);
    private sealed record GpuInfo(string Name, string VramText, string ClockText);
    private sealed record StorageInfo(string Name, string TotalText, string UsedText, double UsedPercent);
    private sealed record DxgiGpuInfo(string Name, ulong DedicatedVideoMemoryBytes);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int EnumAdapters1Delegate(IntPtr factory, uint adapterIndex, out IntPtr adapter);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int GetDesc1Delegate(IntPtr adapter, out DxgiAdapterDesc1 desc);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate uint ComReleaseDelegate(IntPtr comObject);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DxgiAdapterDesc1
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Description;
        public uint VendorId;
        public uint DeviceId;
        public uint SubSysId;
        public uint Revision;
        public UIntPtr DedicatedVideoMemory;
        public UIntPtr DedicatedSystemMemory;
        public UIntPtr SharedSystemMemory;
        public long AdapterLuid;
        public uint Flags;
    }

    [DllImport("dxgi.dll", ExactSpelling = true)]
    private static extern int CreateDXGIFactory1(ref Guid riid, out IntPtr ppFactory);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatusEx lpBuffer);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetLogicalProcessorInformation(IntPtr buffer, ref int returnLength);

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

    [StructLayout(LayoutKind.Sequential)]
    private struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION
    {
        public UIntPtr ProcessorMask;
        public LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
        public ProcessorInformationUnion ProcessorInformation;
    }

    private enum LOGICAL_PROCESSOR_RELATIONSHIP
    {
        RelationProcessorCore = 0,
        RelationNumaNode = 1,
        RelationCache = 2,
        RelationProcessorPackage = 3,
        RelationGroup = 4,
        RelationAll = 0xffff
    }

    [StructLayout(LayoutKind.Explicit)]
    private struct ProcessorInformationUnion
    {
        [FieldOffset(0)] public ProcessorCore ProcessorCore;
        [FieldOffset(0)] public NumaNode NumaNode;
        [FieldOffset(0)] public CacheDescriptor Cache;
        [FieldOffset(0)] private ulong Reserved1;
        [FieldOffset(8)] private ulong Reserved2;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ProcessorCore
    {
        public byte Flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NumaNode
    {
        public uint NodeNumber;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct CacheDescriptor
    {
        public byte Level;
        public byte Associativity;
        public ushort LineSize;
        public uint Size;
        public uint Type;
    }
}
