using Godot;
using System;

public partial class ClayDemo : Node2D
{
    private Clay.ClayRuntime? runtime;
    private ImageTexture? texture;
    private Image? image;
    private uint[]? pixels;

    public override void _Ready()
    {
        runtime = new Clay.ClayRuntime(640, 480, 0xC0FFEE);
        runtime.InstallBuiltinSystems();
        runtime.LoadReactions("{\"rules\": []}");
        runtime.FeedMotion(320, 240, 0, 0);
        runtime.SpawnSpecies("animal", 320, 240, 0.7f, 0.9f, 0.6f, 1, 60);

        image = Image.Create(640, 480, false, Image.Format.Rgba8);
        texture = ImageTexture.CreateFromImage(image);
        pixels = new uint[640 * 480];
    }

    public override void _Process(double delta)
    {
        if (runtime == null || image == null || texture == null || pixels == null)
            return;

        runtime.FeedMotion(GetGlobalMousePosition().X,
                           GetGlobalMousePosition().Y, 0, 0);
        runtime.Step(delta);
        runtime.CopyPixels().CopyTo(pixels);

        byte[] rgba = new byte[pixels.Length * 4];
        for (int i = 0; i < pixels.Length; i++)
        {
            uint p = pixels[i];
            int o = i * 4;
            rgba[o] = (byte)(p >> 16);
            rgba[o + 1] = (byte)(p >> 8);
            rgba[o + 2] = (byte)p;
            rgba[o + 3] = 255;
        }
        image.SetData(640, 480, false, Image.Format.Rgba8, rgba);
        texture.SetImage(image);
        QueueRedraw();
    }

    public override void _Draw()
    {
        if (texture != null)
            DrawTexture(texture, Vector2.Zero);
    }

    public override void _ExitTree()
    {
        runtime?.Dispose();
    }
}
