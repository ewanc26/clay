using Godot;
using System;

public partial class ClayDemo : Node2D
{
    private Clay.ClayRuntime? runtime;
    private ImageTexture? texture;
    private Image? image;
    private byte[]? rgba;

    public override void _Ready()
    {
        runtime = new Clay.ClayRuntime(640, 480, 0xC0FFEE);
        runtime.InstallBuiltinSystems();
        runtime.LoadReactions("{\"rules\": []}");
        runtime.FeedMotion(320, 240, 0, 0);
        runtime.SpawnSpecies("animal", 320, 240, 0.7f, 0.9f, 0.6f, 1, 60);

        image = Image.Create(640, 480, false, Image.Format.Rgba8);
        texture = ImageTexture.CreateFromImage(image);
        rgba = new byte[640 * 480 * 4];
    }

    public override void _Process(double delta)
    {
        if (runtime == null || image == null || texture == null || pixels == null)
            return;

        runtime.FeedMotion(GetGlobalMousePosition().X,
                           GetGlobalMousePosition().Y, 0, 0);
        runtime.Step(delta);
        if (rgba == null) return;
        runtime.CopyRgbaTo(rgba);
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
