using Clay;
using System.Runtime.InteropServices;

if (args.Length != 1)
    throw new ArgumentException("Usage: ClayHostTests NATIVE_LIBRARY_DIRECTORY");
string library = OperatingSystem.IsWindows() ? "clay_engine.dll"
    : OperatingSystem.IsMacOS() ? "libclay_engine.dylib" : "libclay_engine.so";
string nativePath = Path.GetFullPath(Path.Combine(args[0], library));
NativeLibrary.SetDllImportResolver(typeof(ClayRuntime).Assembly,
    (name, assembly, searchPath) => name == "clay_engine"
        ? NativeLibrary.Load(nativePath) : IntPtr.Zero);

static void Require(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}

static T Expect<T>(Action action) where T : Exception
{
    try { action(); }
    catch (T error) { return error; }
    throw new Exception($"Expected {typeof(T).Name}");
}

using var runtime = new ClayRuntime(64, 64, 42);
runtime.InstallBuiltinSystems();
runtime.FeedFocus(true);
Require(runtime.IsFocused, "Focus true did not cross the ABI");
runtime.FeedKeyAt(ClayKey.A, true, 12, 18, ClayModifiers.Shift);
Require(runtime.IsKeyDown(ClayKey.A), "Key press did not cross the ABI");
Require(runtime.CursorX == 12 && runtime.CursorY == 18, "Cursor mismatch");
runtime.FeedFocus(false);
Require(!runtime.IsFocused && !runtime.IsKeyDown(ClayKey.A),
    "Focus loss failed to release held keys");
runtime.LoadScene("{\"version\":1,\"settings\":{\"render\":{\"width\":32,\"height\":24}},\"meshes\":[{\"name\":\"cube\",\"primitive\":\"cube\"}],\"scene\":[{\"component\":\"mesh\",\"mesh\":\"cube\"}]}");
Require(runtime.HasScene && runtime.Width == 32 && runtime.Height == 24,
    "Authored scene did not cross the managed boundary");
runtime.Step(1.0 / 60);
runtime.UnloadScene();
Require(!runtime.HasScene, "Scene did not unload");
string scenePath = Path.Combine(Path.GetTempPath(),
    $"clay-managed-scene-{Guid.NewGuid():N}.clay");
File.WriteAllText(scenePath,
    "{\"version\":1,\"settings\":{\"render\":{\"width\":20,\"height\":12}},\"scene\":[]}");
try
{
    runtime.LoadSceneFile(scenePath);
    Require(runtime.HasScene && runtime.Width == 20 && runtime.Height == 12,
        "Managed scene file did not cross the native ABI");
    runtime.UnloadScene();
}
finally
{
    File.Delete(scenePath);
}
runtime.Resize(32, 24);

string dataPath = Path.Combine(Path.GetTempPath(),
    $"clay-managed-actions-{Guid.NewGuid():N}.json");
File.WriteAllText(dataPath, "{\"actions\":{\"primary\":{\"key\":\"SPACE\"}}}");
string reactionsPath = Path.Combine(Path.GetTempPath(),
    $"clay-managed-reactions-{Guid.NewGuid():N}.json");
File.WriteAllText(reactionsPath, "{\"rules\":[]}");
try
{
    runtime.LoadActionsFile(dataPath);
    runtime.LoadReactionsFile(reactionsPath);
}
finally
{
    File.Delete(dataPath);
    File.Delete(reactionsPath);
}

string audioPath = Path.Combine(Path.GetTempPath(),
    $"clay-managed-audio-{Guid.NewGuid():N}.wav");
File.WriteAllBytes(audioPath, new byte[] {
    (byte)'R', (byte)'I', (byte)'F', (byte)'F', 38, 0, 0, 0,
    (byte)'W', (byte)'A', (byte)'V', (byte)'E',
    (byte)'f', (byte)'m', (byte)'t', (byte)' ', 16, 0, 0, 0,
    1, 0, 1, 0, 0x80, 0xbb, 0, 0, 0x80, 0xbb, 0, 0, 1, 0, 8, 0,
    (byte)'d', (byte)'a', (byte)'t', (byte)'a', 1, 0, 0, 0, 0xff, 0
});
try
{
    uint clip = runtime.LoadWav(audioPath);
    uint voice = runtime.PlayAudio(clip);
    float[] audio = { 9, 9, 9, 9 };
    runtime.MixAudio(audio);
    Require(Math.Abs(audio[0] - (127f / 128f)) < 0.001f
        && Math.Abs(audio[1] - (127f / 128f)) < 0.001f,
        "Managed audio did not cross the native ABI");
    Require(!runtime.StopAudio(voice), "Completed managed audio voice still active");
}
finally
{
    File.Delete(audioPath);
}

runtime.SpawnSpecies("pebble", 32, 24, 1, 0, 0, 1, 60);
runtime.Step(1.0 / 60);
Require(runtime.Frame > 0 && runtime.SimTime > 0, "Runtime did not advance");
uint[] packed = runtime.CopyPixels().ToArray();
byte[] rgba = new byte[packed.Length * 4];
runtime.CopyRgbaTo(rgba);
Require(packed.Distinct().Count() > 1, "Rendered frame is degenerate");
for (int i = 0; i < packed.Length; ++i)
    Require(rgba[4 * i] == ((packed[i] >> 16) & 255)
        && rgba[4 * i + 1] == ((packed[i] >> 8) & 255)
        && rgba[4 * i + 2] == (packed[i] & 255)
        && rgba[4 * i + 3] == 255, "RGBA channel conversion mismatch");

string message = Expect<InvalidOperationException>(
    () => runtime.LoadActions("invalid json")).Message;
Require(message.Contains("Parse") && message.Contains("parse error"),
    "Native error text did not reach the managed exception");
Expect<InvalidOperationException>(() => runtime.Resize(0, 32));
Require(runtime.Width == 32 && runtime.Height == 24,
    "Rejected resize changed dimensions");
runtime.Resize(20, 30);
runtime.Step(1.0 / 60);
Require(runtime.CopyPixels().Length == 600, "Resize left stale managed buffer");
runtime.CopyRgbaTo(new byte[2400]);
Expect<ArgumentException>(() => runtime.CopyRgbaTo(new byte[4]));
runtime.Dispose();
runtime.Dispose();
Expect<ObjectDisposedException>(() => runtime.Step(1.0 / 60));
Console.WriteLine("Managed/native integration passed: input, rendering, audio, errors, resize, disposal.");
