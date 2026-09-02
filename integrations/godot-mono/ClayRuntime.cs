using System;
using System.Runtime.InteropServices;

namespace Clay;

/// <summary>Managed owner for a native Clay runtime.</summary>
public sealed class ClayRuntime : IDisposable
{
    private sealed class RuntimeHandle : SafeHandle
    {
        private RuntimeHandle() : base(IntPtr.Zero, true) { }
        public override bool IsInvalid => IsClosed || handle == IntPtr.Zero;
        protected override bool ReleaseHandle()
        {
            Native.Destroy(handle);
            return true;
        }
    }

    private readonly RuntimeHandle handle;
    private readonly uint[] pixelBuffer;

    public ClayRuntime(int width, int height, ulong seed)
    {
        handle = Native.Create(width, height, seed);
        if (handle.IsInvalid)
            throw new InvalidOperationException("Clay runtime creation failed.");
        pixelBuffer = new uint[checked(width * height)];
    }

    public int Width => Native.Width(handle);
    public int Height => Native.Height(handle);
    public ulong Frame => Native.Frame(handle);

    public void InstallBuiltinSystems() => Check(
        Native.InstallBuiltinSystems(handle));

    public void LoadReactions(string json) => Check(
        Native.LoadReactions(handle, json ?? throw new ArgumentNullException(nameof(json))));

    public void SpawnSpecies(string species, float x, float y, float r, float g,
                             float b, float a, float life) => Check(
        Native.SpawnSpecies(handle,
            species ?? throw new ArgumentNullException(nameof(species)), x, y,
            r, g, b, a, life));

    public void FeedKey(int key, bool pressed) => Check(
        Native.FeedKey(handle, key, pressed));

    public void FeedMotion(double x, double y, double dx, double dy) => Check(
        Native.FeedMotion(handle, x, y, dx, dy));

    public void Step(double deltaSeconds) => Check(
        Native.Step(handle, deltaSeconds));

    /// <summary>Copies packed 0x00RRGGBB pixels into a caller-owned array.</summary>
    public ReadOnlySpan<uint> CopyPixels()
    {
        IntPtr pixels = Native.Pixels(handle, out nuint count);
        if (pixels == IntPtr.Zero || count != (nuint)pixelBuffer.Length)
            throw new InvalidOperationException("Clay framebuffer is unavailable.");
        // Marshal.Copy has no UInt32 overload on every supported runtime.
        for (int i = 0; i < pixelBuffer.Length; i++)
            pixelBuffer[i] = unchecked((uint)Marshal.ReadInt32(pixels, i * sizeof(uint)));
        return pixelBuffer;
    }

    public void CopyPixelsTo(uint[] destination)
    {
        if (destination == null) throw new ArgumentNullException(nameof(destination));
        if (destination.Length != pixelBuffer.Length)
            throw new ArgumentException("Destination has the wrong pixel count.",
                                        nameof(destination));
        CopyPixels().CopyTo(destination);
    }

    public void Dispose() => handle.Dispose();

    private static void Check(int error)
    {
        if (error != 0) throw new InvalidOperationException($"Clay error: {error}");
    }

    private static class Native
    {
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_create")]
        public static extern RuntimeHandle Create(int width, int height, ulong seed);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_destroy")]
        public static extern void Destroy(IntPtr runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_step")]
        public static extern int Step(RuntimeHandle runtime, double dt);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_key")]
        public static extern int FeedKey(RuntimeHandle runtime, int key,
                                         [MarshalAs(UnmanagedType.I1)] bool pressed);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_motion")]
        public static extern int FeedMotion(RuntimeHandle runtime, double x, double y, double dx, double dy);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_reactions")]
        public static extern int LoadReactions(RuntimeHandle runtime, string json);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_spawn_species")]
        public static extern int SpawnSpecies(RuntimeHandle runtime, string species,
                                               float x, float y, float r, float g,
                                               float b, float a, float life);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_install_builtin_systems")]
        public static extern int InstallBuiltinSystems(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_width")]
        public static extern int Width(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_height")]
        public static extern int Height(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_frame")]
        public static extern ulong Frame(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_pixels")]
        public static extern IntPtr Pixels(RuntimeHandle runtime, out nuint count);
    }
}
