using SmartDisplayPcAgent.Models;
using SmartDisplayPcAgent.Resources;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.UI.Notifications;
using Windows.UI.Notifications.Management;

namespace SmartDisplayPcAgent.Services;

public sealed class WindowsNotificationForwarderService : IDisposable
{
    private readonly HashSet<uint> _seenNotificationIds = [];
    private CancellationTokenSource? _cts;
    private Task? _worker;

    private static string S(string name) => Strings.Get(name);

    public void Start(
        Func<bool> shouldForward,
        Func<WindowsForwardedNotification, CancellationToken, Task<bool>> forwardAsync,
        Action<string> updateStatus)
    {
        if (_worker is not null)
            return;

        _cts = new CancellationTokenSource();
        _worker = Task.Run(
            () => RunAsync(shouldForward, forwardAsync, updateStatus, _cts.Token),
            _cts.Token);
    }

    private async Task RunAsync(
        Func<bool> shouldForward,
        Func<WindowsForwardedNotification, CancellationToken, Task<bool>> forwardAsync,
        Action<string> updateStatus,
        CancellationToken cancellationToken)
    {
        if (!OperatingSystem.IsWindowsVersionAtLeast(10, 0, 17763))
        {
            updateStatus(S("WindowsNotificationsUnavailableVersion"));
            return;
        }

        UserNotificationListener listener;
        try
        {
            listener = UserNotificationListener.Current;
        }
        catch (Exception ex)
        {
            updateStatus(string.Format(S("WindowsNotificationsUnavailableFormat"), ex.Message));
            return;
        }

        UserNotificationListenerAccessStatus accessStatus;
        try
        {
            accessStatus = await listener.RequestAccessAsync().AsTask(cancellationToken);
        }
        catch (Exception ex)
        {
            updateStatus(string.Format(S("WindowsNotificationPermissionFailedFormat"), ex.Message));
            return;
        }

        if (accessStatus != UserNotificationListenerAccessStatus.Allowed)
        {
            updateStatus(string.Format(S("WindowsNotificationPermissionFormat"), accessStatus));
            return;
        }

        updateStatus(S("WindowsNotificationsListening"));

        await MarkExistingNotificationsSeenAsync(listener, cancellationToken);

        while (!cancellationToken.IsCancellationRequested)
        {
            try
            {
                await ForwardNewNotificationsAsync(listener, shouldForward, forwardAsync, cancellationToken);
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception ex)
            {
                updateStatus(string.Format(S("WindowsNotificationsReadErrorFormat"), ex.Message));
            }

            try
            {
                await Task.Delay(TimeSpan.FromSeconds(2), cancellationToken);
            }
            catch (OperationCanceledException)
            {
                break;
            }
        }
    }

    private async Task MarkExistingNotificationsSeenAsync(
        UserNotificationListener listener,
        CancellationToken cancellationToken)
    {
        var notifications = await listener.GetNotificationsAsync(NotificationKinds.Toast).AsTask(cancellationToken);
        foreach (var notification in notifications)
            _seenNotificationIds.Add(notification.Id);
    }

    private async Task ForwardNewNotificationsAsync(
        UserNotificationListener listener,
        Func<bool> shouldForward,
        Func<WindowsForwardedNotification, CancellationToken, Task<bool>> forwardAsync,
        CancellationToken cancellationToken)
    {
        var notifications = await listener.GetNotificationsAsync(NotificationKinds.Toast).AsTask(cancellationToken);

        foreach (var notification in notifications.OrderBy(item => item.CreationTime))
        {
            if (!_seenNotificationIds.Add(notification.Id))
                continue;

            if (!shouldForward())
                continue;

            WindowsForwardedNotification? forwarded = TryCreateForwardedNotification(notification);
            if (forwarded is null)
                continue;

            await forwardAsync(forwarded, cancellationToken);
        }
    }

    private static WindowsForwardedNotification? TryCreateForwardedNotification(UserNotification notification)
    {
        string appName = notification.AppInfo.DisplayInfo.DisplayName;
        if (string.IsNullOrWhiteSpace(appName))
            appName = "Windows";

        var binding = notification.Notification.Visual.GetBinding(KnownNotificationBindings.ToastGeneric);
        var textLines = binding?.GetTextElements()
            .Select(element => element.Text?.Trim())
            .Where(text => !string.IsNullOrWhiteSpace(text))
            .Take(2)
            .ToArray() ?? [];

        if (textLines.Length == 0)
            return null;

        string sender = textLines.Length >= 2 ? textLines[0]! : appName;
        string title = textLines.Length >= 2 ? textLines[1]! : textLines[0]!;

        return new WindowsForwardedNotification(
            AppName: Truncate(appName, 22),
            Sender: Truncate(sender, 34),
            Title: Truncate(title, 60),
            Time: notification.CreationTime.LocalDateTime.ToString("HH:mm"),
            Accent: AccentForApp(appName),
            DurationMs: 5000);
    }

    private static string AccentForApp(string appName)
    {
        string value = appName.ToLowerInvariant();
        if (value.Contains("teams")) return "#5B5FC7";
        if (value.Contains("outlook") || value.Contains("mail")) return "#0078D4";
        if (value.Contains("whatsapp")) return "#25D366";
        if (value.Contains("discord")) return "#5865F2";
        return "#55DFFF";
    }

    private static string Truncate(string value, int maxLength)
    {
        value = value.Trim();
        return value.Length <= maxLength ? value : value[..maxLength];
    }

    public void Dispose()
    {
        _cts?.Cancel();
        _worker = null;
        _cts?.Dispose();
    }
}
