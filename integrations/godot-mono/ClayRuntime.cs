using System;
using System.Runtime.InteropServices;

namespace Clay;

/// <summary>Error codes returned by the Clay C ABI.</summary>
public enum ClayError
{
    Ok = 0,
    OutOfMemory,
    Parse,
    NotFound,
    TypeMismatch,
    InvalidArgument,
    Io,
    Full,
    Overflow,
}

public enum ClayAudioBus
{
    Sfx = 0,
    Music = 1,
}

/// <summary>Managed owner for a native Clay runtime.</summary>
public sealed class ClayRuntime : IDisposable
{
    public const uint AbiVersion = 1;

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
    private uint[] pixelBuffer;

    public ClayRuntime(int width, int height, ulong seed)
        : this(width, height, seed, 4u << 20) { }

    public ClayRuntime(int width, int height, ulong seed, nuint arenaBytes)
    {
        if (Native.AbiVersion() != AbiVersion)
            throw new InvalidOperationException("Incompatible Clay native ABI.");
        handle = Native.CreateWithArena(width, height, seed, arenaBytes);
        if (handle.IsInvalid)
            throw new InvalidOperationException("Clay runtime creation failed.");
        pixelBuffer = new uint[checked(width * height)];
    }

    public int Width => Native.Width(handle);
    public int Height => Native.Height(handle);
    public ulong Frame => Native.Frame(handle);
    public ulong Seed => Native.Seed(handle);
    public double SimTime => Native.SimTime(handle);
    public double SimDelta => Native.SimDelta(handle);
    public double TimeScale => Native.TimeScale(handle);
    public double CursorX => Native.CursorX(handle);
    public double CursorY => Native.CursorY(handle);
    public nuint RecordingCount => Native.RecordingCount(handle);
    public ulong RecordingFingerprint => Native.RecordingFingerprint(handle);
    public bool IsReplaying => Native.IsReplaying(handle);
    public bool IsFocused => Native.IsFocused(handle);

    public void InstallBuiltinSystems() => Check(
        Native.InstallBuiltinSystems(handle));

    public void LoadReactions(string json) => Check(
        Native.LoadReactions(handle, json ?? throw new ArgumentNullException(nameof(json))));

    public void LoadReactionsFile(string path) => Check(
        Native.LoadReactionsFile(handle,
            path ?? throw new ArgumentNullException(nameof(path))));

    public void LoadActions(string json) => Check(
        Native.LoadActions(handle, json ?? throw new ArgumentNullException(nameof(json))));

    public void LoadActionsFile(string path) => Check(
        Native.LoadActionsFile(handle,
            path ?? throw new ArgumentNullException(nameof(path))));

    public void LoadScene(string json)
    {
        Check(Native.LoadScene(handle,
            json ?? throw new ArgumentNullException(nameof(json))));
        pixelBuffer = new uint[checked(Width * Height)];
    }

    public void LoadSceneFile(string path)
    {
        Check(Native.LoadSceneFile(handle,
            path ?? throw new ArgumentNullException(nameof(path))));
        pixelBuffer = new uint[checked(Width * Height)];
    }

    public bool HasScene => Native.HasScene(handle);

    public void UnloadScene() => Native.UnloadScene(handle);

    public void SaveRecording(string path) => Check(
        Native.SaveRecording(handle, path ?? throw new ArgumentNullException(nameof(path))));

    public void LoadRecording(string path) => Check(
        Native.LoadRecording(handle, path ?? throw new ArgumentNullException(nameof(path))));

    public void SetReplaying(bool replaying) => Native.SetReplaying(handle, replaying);

    public void SpawnSpecies(string species, float x, float y, float r, float g,
                             float b, float a, float life) => Check(
        Native.SpawnSpecies(handle,
            species ?? throw new ArgumentNullException(nameof(species)), x, y,
            r, g, b, a, life));

    public void SpawnRipple(float x, float y, float radius, float r, float g,
                            float b, float a) => Check(
        Native.SpawnRipple(handle, x, y, radius, r, g, b, a));

    public void FeedKey(int key, bool pressed) => Check(
        Native.FeedKey(handle, key, pressed));

    public void FeedKey(ClayKey key, bool pressed) => FeedKey((int)key, pressed);

    public void FeedKeyAt(ClayKey key, bool pressed, double x, double y,
                          int mods = 0) => Check(
        Native.FeedKeyAt(handle, (int)key, pressed, x, y, mods));

    public void FeedKeyAt(ClayKey key, bool pressed, double x, double y,
                          ClayModifiers mods) => FeedKeyAt(key, pressed, x, y,
                                                           (int)mods);

    public bool IsKeyDown(ClayKey key) => Native.IsKeyDown(handle, (int)key);

    public void FeedMotion(double x, double y, double dx, double dy) => Check(
        Native.FeedMotion(handle, x, y, dx, dy));

    public void FeedWheel(double x, double y, int clicks) => Check(
        Native.FeedWheel(handle, x, y, clicks));

    public void FeedFocus(bool focused) => Check(
        Native.FeedFocus(handle, focused));

    public void SetTimeScale(double scale) => Native.SetTimeScale(handle, scale);

    public void Step(double deltaSeconds) => Check(
        Native.Step(handle, deltaSeconds));

    public void Resize(int width, int height) => Check(
        ResizeNative(width, height));

    private int ResizeNative(int width, int height)
    {
        int error = Native.Resize(handle, width, height);
        if (error == 0)
            pixelBuffer = new uint[checked(width * height)];
        return error;
    }

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

    public void CopyRgbaTo(byte[] destination)
    {
        if (destination == null) throw new ArgumentNullException(nameof(destination));
        int expected = checked(pixelBuffer.Length * 4);
        if (destination.Length != expected)
            throw new ArgumentException("Destination has the wrong byte count.",
                                        nameof(destination));
        IntPtr pixels = Native.PixelsRgba(handle, out nuint count);
        if (pixels == IntPtr.Zero || count != (nuint)expected)
            throw new InvalidOperationException("Clay framebuffer is unavailable.");
        Marshal.Copy(pixels, destination, 0, destination.Length);
    }

    /// <summary>Writes the latest rendered frame as an RGB PNG.</summary>
    public void SavePng(string path) => Check(
        Native.SavePng(handle, path ?? throw new ArgumentNullException(nameof(path))));

    public uint LoadWav(string path)
    {
        if (path == null) throw new ArgumentNullException(nameof(path));
        Check(Native.AudioLoadWav(handle, path, out uint clip));
        return clip;
    }

    public uint LoadAudioFile(string path)
    {
        if (path == null) throw new ArgumentNullException(nameof(path));
        Check(Native.AudioLoadFile(handle, path, out uint clip));
        return clip;
    }

    public bool UnloadAudio(uint clip) => Native.AudioUnloadClip(handle, clip);

    public uint PlayAudio(uint clip, ClayAudioBus bus = ClayAudioBus.Sfx,
                          bool loop = false, float gain = 1)
    {
        uint voice = Native.AudioPlay(handle, clip, (int)bus, loop, gain);
        if (voice == 0)
            throw new InvalidOperationException("Clay audio playback failed.");
        return voice;
    }

    public bool StopAudio(uint voice) => Native.AudioStop(handle, voice);

    public bool PauseAudio(uint voice) => Native.AudioPause(handle, voice);

    public bool ResumeAudio(uint voice) => Native.AudioResume(handle, voice);

    public bool IsAudioPlaying(uint voice) => Native.AudioVoiceActive(handle, voice);

    public nuint GetAudioClipFrames(uint clip) =>
        Native.AudioClipFrameCount(handle, clip);

    public uint AudioSampleRate => Native.AudioSampleRate(handle);

    public void MixAudio(float[] samples)
    {
        if (samples == null) throw new ArgumentNullException(nameof(samples));
        Check(Native.AudioMixStereo(handle, samples, (nuint)samples.Length));
    }

    public void SetMasterGain(float gain) => Native.AudioSetMasterGain(handle, gain);

    public void SetBusGain(ClayAudioBus bus, float gain) =>
        Native.AudioSetBusGain(handle, (int)bus, gain);

    public void Dispose() => handle.Dispose();

    private static void Check(int error)
    {
        if (error != 0)
        {
            ClayError code = (ClayError)error;
            string message = Marshal.PtrToStringAnsi(Native.ErrorString(error))
                             ?? code.ToString();
            throw new InvalidOperationException(
                $"Clay error: {error} ({code}): {message}.");
        }
    }

    private static class Native
    {
        [DllImport("clay_engine", EntryPoint = "cl_engine_error_string")]
        public static extern IntPtr ErrorString(int error);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_abi_version")]
        public static extern uint AbiVersion();
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_create")]
        public static extern RuntimeHandle Create(int width, int height, ulong seed);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_create_with_arena")]
        public static extern RuntimeHandle CreateWithArena(int width, int height,
                                                            ulong seed,
                                                            nuint arenaBytes);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_destroy")]
        public static extern void Destroy(IntPtr runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_step")]
        public static extern int Step(RuntimeHandle runtime, double dt);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_resize")]
        public static extern int Resize(RuntimeHandle runtime, int width, int height);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_key")]
        public static extern int FeedKey(RuntimeHandle runtime, int key,
                                         [MarshalAs(UnmanagedType.I1)] bool pressed);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_key_at")]
        public static extern int FeedKeyAt(RuntimeHandle runtime, int key,
                                            [MarshalAs(UnmanagedType.I1)] bool pressed,
                                            double x, double y, int mods);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_motion")]
        public static extern int FeedMotion(RuntimeHandle runtime, double x, double y, double dx, double dy);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_wheel")]
        public static extern int FeedWheel(RuntimeHandle runtime, double x, double y, int wheel);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_feed_focus")]
        public static extern int FeedFocus(RuntimeHandle runtime, [MarshalAs(UnmanagedType.I1)] bool focused);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_is_key_down")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool IsKeyDown(RuntimeHandle runtime, int key);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_is_focused")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool IsFocused(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_set_time_scale")]
        public static extern void SetTimeScale(RuntimeHandle runtime, double scale);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_reactions")]
        public static extern int LoadReactions(RuntimeHandle runtime, string json);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_reactions_file")]
        public static extern int LoadReactionsFile(RuntimeHandle runtime, string path);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_actions")]
        public static extern int LoadActions(RuntimeHandle runtime, string json);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_actions_file")]
        public static extern int LoadActionsFile(RuntimeHandle runtime, string path);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_scene")]
        public static extern int LoadScene(RuntimeHandle runtime, string json);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_scene_file")]
        public static extern int LoadSceneFile(RuntimeHandle runtime, string path);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_unload_scene")]
        public static extern void UnloadScene(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_has_scene")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool HasScene(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_save_recording")]
        public static extern int SaveRecording(RuntimeHandle runtime, string path);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_load_recording")]
        public static extern int LoadRecording(RuntimeHandle runtime, string path);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_set_replaying")]
        public static extern void SetReplaying(RuntimeHandle runtime,
                                                [MarshalAs(UnmanagedType.I1)] bool replaying);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_is_replaying")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool IsReplaying(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_recording_count")]
        public static extern nuint RecordingCount(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_recording_fingerprint")]
        public static extern ulong RecordingFingerprint(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_spawn_species")]
        public static extern int SpawnSpecies(RuntimeHandle runtime, string species,
                                               float x, float y, float r, float g,
                                               float b, float a, float life);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_spawn_ripple")]
        public static extern int SpawnRipple(RuntimeHandle runtime, float x, float y,
                                              float radius, float r, float g,
                                              float b, float a);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_install_builtin_systems")]
        public static extern int InstallBuiltinSystems(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_width")]
        public static extern int Width(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_height")]
        public static extern int Height(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_frame")]
        public static extern ulong Frame(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_seed")]
        public static extern ulong Seed(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_sim_time")]
        public static extern double SimTime(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_sim_dt")]
        public static extern double SimDelta(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_time_scale")]
        public static extern double TimeScale(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_cursor_x")]
        public static extern double CursorX(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_cursor_y")]
        public static extern double CursorY(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_pixels")]
        public static extern IntPtr Pixels(RuntimeHandle runtime, out nuint count);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_pixels_rgba")]
        public static extern IntPtr PixelsRgba(RuntimeHandle runtime, out nuint byteCount);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_save_png")]
        public static extern int SavePng(RuntimeHandle runtime, string path);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_load_wav")]
        public static extern int AudioLoadWav(RuntimeHandle runtime, string path,
                                               out uint clip);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_load_file")]
        public static extern int AudioLoadFile(RuntimeHandle runtime, string path,
                                                out uint clip);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_play")]
        public static extern uint AudioPlay(RuntimeHandle runtime, uint clip,
                                             int bus,
                                             [MarshalAs(UnmanagedType.I1)] bool loop,
                                             float gain);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_unload_clip")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool AudioUnloadClip(RuntimeHandle runtime, uint clip);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_stop")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool AudioStop(RuntimeHandle runtime, uint voice);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_pause")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool AudioPause(RuntimeHandle runtime, uint voice);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_resume")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool AudioResume(RuntimeHandle runtime, uint voice);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_voice_active")]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool AudioVoiceActive(RuntimeHandle runtime, uint voice);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_clip_frame_count")]
        public static extern nuint AudioClipFrameCount(RuntimeHandle runtime, uint clip);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_sample_rate")]
        public static extern uint AudioSampleRate(RuntimeHandle runtime);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_mix_stereo")]
        public static extern int AudioMixStereo(RuntimeHandle runtime,
                                                 [In, Out] float[] samples,
                                                 nuint sampleCount);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_set_master_gain")]
        public static extern void AudioSetMasterGain(RuntimeHandle runtime,
                                                      float gain);
        [DllImport("clay_engine", EntryPoint = "cl_engine_runtime_audio_set_bus_gain")]
        public static extern void AudioSetBusGain(RuntimeHandle runtime, int bus,
                                                  float gain);
    }
}
