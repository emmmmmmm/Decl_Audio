using System;
using System.Runtime.InteropServices;
using System.Text;

#nullable enable

namespace DeclAudio;

public enum DeclAudioBackend : uint
{
    Silent = 0,
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

    // per-instance storage caps
    public uint MaxProgramNodeCount;
    public uint MaxProgramConcurrentVoices;
    public uint MaxProgramParameterSlotCount;

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

    public bool LoadBank(string sourcePath)
        => NativeMethods.LoadBank(_handle, sourcePath);

    public bool LoadBankAsync(string sourcePath)
        => NativeMethods.LoadBankAsync(_handle, sourcePath);

    public void UnloadBank(string sourcePath)
        => NativeMethods.UnloadBank(_handle, sourcePath);

    public void Update()
        => NativeMethods.Update(_handle);

    public string? TryDequeueLog()
        => NativeMethods.TryDequeueLog(_handle);

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

    public void SetGlobalTag(string tag)
        => NativeMethods.SetGlobalTag(_handle, tag);

    public void RemoveGlobalTag(string tag)
        => NativeMethods.RemoveGlobalTag(_handle, tag);

    public void SetGlobalValue(string parameter, float value)
        => NativeMethods.SetGlobalValue(_handle, parameter, value);

    public void SetMasterGain(float gain)
        => NativeMethods.SetMasterGain(_handle, gain);

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
    private const int DeclAudioLogMessageMaxLength = 512;
    private const int DeclAudioLogMessageStructSize = sizeof(uint) + DeclAudioLogMessageMaxLength;

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

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool LoadBank(IntPtr engine, string sourcePath);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool LoadBankAsync(IntPtr engine, string sourcePath);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void UnloadBank(IntPtr engine, string sourcePath);

    [LibraryImport(Dll)]
    internal static partial void Update(IntPtr engine);

    [LibraryImport(Dll, EntryPoint = "TryDequeueLog")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static partial bool TryDequeueLogNative(IntPtr engine, IntPtr outMessage);

    internal static string? TryDequeueLog(IntPtr engine)
    {
        IntPtr nativeMessage = Marshal.AllocHGlobal(DeclAudioLogMessageStructSize);
        try
        {
            if (!TryDequeueLogNative(engine, nativeMessage))
                return null;

            int utf8Length = Marshal.ReadInt32(nativeMessage, 0);
            if (utf8Length < 0 || utf8Length >= DeclAudioLogMessageMaxLength)
                throw new InvalidOperationException($"Native TryDequeueLog returned an invalid message length: {utf8Length}.");

            byte[] utf8Bytes = new byte[utf8Length];
            if (utf8Length > 0)
                Marshal.Copy(IntPtr.Add(nativeMessage, sizeof(uint)), utf8Bytes, 0, utf8Length);

            return Encoding.UTF8.GetString(utf8Bytes);
        }
        finally
        {
            Marshal.FreeHGlobal(nativeMessage);
        }
    }

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
    internal static partial void SetGlobalTag(IntPtr engine, string tag);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void RemoveGlobalTag(IntPtr engine, string tag);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void SetGlobalValue(IntPtr engine, string parameter, float value);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial void DestroyEntity(IntPtr engine, string entityId);

    [LibraryImport(Dll)]
    internal static partial void SetMasterGain(IntPtr engine, float gain);
}
