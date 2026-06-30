namespace SmartDisplayPcAgent.Models;

public sealed record DeviceLocationOption(
    string Label,
    double Latitude,
    double Longitude,
    string Timezone,
    string CountryCode)
{
    public string CoordinateText => $"{Latitude:0.0000}, {Longitude:0.0000}";
}
