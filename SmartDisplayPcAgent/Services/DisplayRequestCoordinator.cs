using System;
using System.Threading;
using System.Threading.Tasks;

namespace SmartDisplayPcAgent.Services;

internal static class DisplayRequestCoordinator
{
    private static readonly SemaphoreSlim RequestGate = new(1, 1);

    public static DateTime LastSuccessfulRequestUtc { get; private set; } = DateTime.MinValue;

    public readonly record struct CoordinatedResult<T>(bool Skipped, T? Value);

    public static async Task<bool?> TryRunBoolAsync(
        Func<CancellationToken, Task<bool>> action,
        CancellationToken cancellationToken)
    {
        if (!RequestGate.Wait(0))
            return null;

        try
        {
            bool result = await action(cancellationToken);
            if (result) LastSuccessfulRequestUtc = DateTime.UtcNow;
            return result;
        }
        finally
        {
            RequestGate.Release();
        }
    }

    public static async Task<bool> RunBoolAsync(
        Func<CancellationToken, Task<bool>> action,
        TimeSpan waitTimeout,
        CancellationToken cancellationToken)
    {
        bool entered;
        try
        {
            entered = await RequestGate.WaitAsync(waitTimeout, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return false;
        }

        if (!entered)
            return false;

        try
        {
            bool result = await action(cancellationToken);
            if (result) LastSuccessfulRequestUtc = DateTime.UtcNow;
            return result;
        }
        finally
        {
            RequestGate.Release();
        }
    }

    public static async Task<T?> TryRunAsync<T>(
        Func<CancellationToken, Task<T?>> action,
        CancellationToken cancellationToken)
        where T : class
    {
        if (!RequestGate.Wait(0))
            return null;

        try
        {
            T? result = await action(cancellationToken);
            if (result is not null) LastSuccessfulRequestUtc = DateTime.UtcNow;
            return result;
        }
        finally
        {
            RequestGate.Release();
        }
    }

    public static async Task<CoordinatedResult<T>> TryRunResultAsync<T>(
        Func<CancellationToken, Task<T?>> action,
        CancellationToken cancellationToken)
        where T : class
    {
        if (!RequestGate.Wait(0))
            return new CoordinatedResult<T>(true, null);

        try
        {
            T? result = await action(cancellationToken);
            if (result is not null) LastSuccessfulRequestUtc = DateTime.UtcNow;
            return new CoordinatedResult<T>(false, result);
        }
        finally
        {
            RequestGate.Release();
        }
    }

    public static async Task<T?> RunAsync<T>(
        Func<CancellationToken, Task<T?>> action,
        TimeSpan waitTimeout,
        CancellationToken cancellationToken)
        where T : class
    {
        bool entered;
        try
        {
            entered = await RequestGate.WaitAsync(waitTimeout, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return null;
        }

        if (!entered)
            return null;

        try
        {
            T? result = await action(cancellationToken);
            if (result is not null) LastSuccessfulRequestUtc = DateTime.UtcNow;
            return result;
        }
        finally
        {
            RequestGate.Release();
        }
    }

    public static bool HasRecentSuccess(TimeSpan window)
    {
        return LastSuccessfulRequestUtc != DateTime.MinValue &&
               DateTime.UtcNow - LastSuccessfulRequestUtc <= window;
    }
}
