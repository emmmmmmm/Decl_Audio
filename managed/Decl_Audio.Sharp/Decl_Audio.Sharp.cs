using System.Runtime.InteropServices;

namespace DeclAudio;

public enum DeclAudioBackend : uint
{
    Silent          = 0,
    PlatformDefault = 1,
}

[StructLayout(LayoutKind.Sequential)]
public struct EngineConfig
{
    public uint SampleRate;
    public uint OutputChannelCount;
    public uint CallbackFrameCount;

    public uint MaxInstances;
    public uint MaxBlockFrames;
    public uint CommandQueueCapacity;
    public uint HostQueueCapacity;

    public DeclAudioBackend Backend;
}

public sealed class AudioEngine : IDisposable
{
    private IntPtr _handle;
    private bool _disposed;

    private AudioEngine(IntPtr handle) => _handle = handle;

    public static uint ApiVersion() => NativeMethods.GetApiVersion();

    public static EngineConfig DefaultConfig() => NativeMethods.GetDefaultConfig();

    public static AudioEngine Create(in EngineConfig config)
    {
        if (!NativeMethods.CreateEngine(in config, out IntPtr handle))
            throw new InvalidOperationException("CreateEngine failed.");
        return new AudioEngine(handle);
    }

    public bool LoadBehaviors(string sourcePath)
        => NativeMethods.LoadBehaviors(_handle, sourcePath);

    public void Update()
        => NativeMethods.Update(_handle);

    public void SetTag(string entityId, string tag)
        => NativeMethods.SetTag(_handle, entityId, tag);

    public void RemoveTag(string entityId, string tag)
        => NativeMethods.RemoveTag(_handle, entityId, tag);

    public void SetTransientTag(string entityId, string tag)
        => NativeMethods.SetTransientTag(_handle, entityId, tag);

    public void SetValue(string entityId, string parameter, float value)
        => NativeMethods.SetValue(_handle, entityId, parameter, value);

    public void SetQuatValue(string entityId, string key, float a, float b, float c, float d)
        => NativeMethods.SetQuatValue(_handle, entityId, key, a, b, c, d);

    public void SetPosition(string entityId, float x, float y, float z)
        => NativeMethods.SetPosition(_handle, entityId, x, y, z);

    public void SetListenerPosition(float x, float y, float z)
        => NativeMethods.SetListenerPosition(_handle, x, y, z);

    public void SetTransform(string entityId, float x, float y, float z, float a, float b, float c, float d)
        => NativeMethods.SetTransform(_handle, entityId, x, y, z, a, b, c, d);

    public void DestroyEntity(string entityId)
        => NativeMethods.DestroyEntity(_handle, entityId);

    public void Dispose()
    {
        if (_disposed) return;
        NativeMethods.DestroyEngine(_handle);
        _handle = IntPtr.Zero;
        _disposed = true;
    }
}

#pragma warning disable IDE0060 // LibraryImport partial method params are consumed by source generation
internal static partial class NativeMethods
{
    private const string Dll = "Decl_Audio";

    [LibraryImport(Dll)]
    internal static partial uint GetApiVersion();

    [LibraryImport(Dll)]
    internal static partial EngineConfig GetDefaultConfig();

    [LibraryImport(Dll)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool CreateEngine(
        in EngineConfig config,
        out IntPtr outEngine);

    [LibraryImport(Dll)]
    internal static partial void DestroyEngine(IntPtr engine);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool LoadBehaviors(IntPtr engine, string sourcePath);

    [LibraryImport(Dll)]
    internal static partial void Update(IntPtr engine);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetTag(IntPtr engine, string entityId, string tag);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void RemoveTag(IntPtr engine, string entityId, string tag);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetTransientTag(IntPtr engine, string entityId, string tag);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetValue(IntPtr engine, string entityId, string parameter, float value);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetQuatValue(IntPtr engine, string entityId, string key, float a, float b, float c, float d);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetPosition(IntPtr engine, string entityId, float x, float y, float z);

    [LibraryImport(Dll)]
    internal static partial void SetListenerPosition(IntPtr engine, float x, float y, float z);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetTransform(IntPtr engine, string entityId, float x, float y, float z, float a, float b, float c, float d);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void DestroyEntity(IntPtr engine, string entityId);
}
