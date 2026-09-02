using Godot;
using System;

public partial class ClayDemo : Node2D
{
    private Clay.ClayRuntime? runtime;
    private ImageTexture? texture;
    private Image? image;
    private byte[]? rgba;
    private int surfaceWidth;
    private int surfaceHeight;

    public override void _Ready()
    {
        runtime = new Clay.ClayRuntime(640, 480, 0xC0FFEE);
        runtime.InstallBuiltinSystems();
        runtime.LoadReactions("{\"rules\": []}");
        runtime.FeedMotion(320, 240, 0, 0);
        runtime.SpawnSpecies("animal", 320, 240, 0.7f, 0.9f, 0.6f, 1, 60);

        ResizeSurface(640, 480);
    }

    public override void _Process(double delta)
    {
        if (runtime == null || image == null || texture == null || rgba == null)
            return;

        Vector2 viewport = GetViewportRect().Size;
        int width = Math.Max(1, (int)viewport.X);
        int height = Math.Max(1, (int)viewport.Y);
        if (width != surfaceWidth || height != surfaceHeight)
            ResizeSurface(width, height);

        runtime.FeedMotion(GetGlobalMousePosition().X,
                           GetGlobalMousePosition().Y, 0, 0);
        runtime.Step(delta);
        runtime.CopyRgbaTo(rgba);
        image.SetData(surfaceWidth, surfaceHeight, false, Image.Format.Rgba8,
                      rgba);
        texture.SetImage(image);
        QueueRedraw();
    }

    public override void _Input(InputEvent @event)
    {
        if (runtime == null)
            return;

        if (@event is InputEventKey keyEvent && keyEvent.Keycode == Key.Space)
        {
            runtime.FeedKeyAt(Clay.ClayKey.Space, keyEvent.Pressed,
                              runtime.CursorX, runtime.CursorY);
        }
        else if (@event is InputEventMouseButton mouseEvent &&
                 mouseEvent.ButtonIndex == MouseButton.Left)
        {
            runtime.FeedKeyAt(Clay.ClayKey.MouseLeft, mouseEvent.Pressed,
                              mouseEvent.Position.X, mouseEvent.Position.Y);
        }
    }

    private void ResizeSurface(int width, int height)
    {
        if (runtime != null)
            runtime.Resize(width, height);
        surfaceWidth = width;
        surfaceHeight = height;
        image = Image.CreateEmpty(width, height, false, Image.Format.Rgba8);
        texture = ImageTexture.CreateFromImage(image);
        rgba = new byte[checked(width * height * 4)];
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
