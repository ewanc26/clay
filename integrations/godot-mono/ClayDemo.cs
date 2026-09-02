using Godot;
using System;

public partial class ClayDemo : Node2D
{
    private const string SampleReactions = """
        {
          "rules": [
            { "name": "click ripple", "on": "input.key",
              "match": { "value": "MOUSE_LEFT", "kind": "press" },
              "cooldown": 0.05,
              "do": [ { "effect": "ripple", "color": [0.95, 0.72, 0.40], "radius": 42 } ] },
            { "name": "space bloom", "on": "input.key",
              "match": { "value": "SPACE", "kind": "press" },
              "cooldown": 0.15,
              "do": [
                { "effect": "flash", "color": [1.0, 0.85, 0.55] },
                { "effect": "ripple", "color": [0.95, 0.55, 0.40], "radius": 90 }
              ] },
            { "name": "wheel ripple", "on": "input.wheel", "cooldown": 0.2,
              "do": [ { "effect": "ripple", "color": [0.55, 0.75, 0.95], "radius": 70 } ] },
            { "name": "motion moss", "on": "input.motion", "cooldown": 0.35,
              "do": [ { "effect": "spawn", "species": "pebble", "life": 6.0,
                         "color": [0.45, 0.55, 0.40] } ] },
            { "name": "grow herd", "on": "input.key",
              "match": { "value": "E", "kind": "press" }, "cooldown": 0.2,
              "do": [ { "effect": "spawn", "species": "animal", "life": 60.0,
                         "color": [0.70, 0.85, 0.60] } ] },
            { "name": "calm herd", "on": "input.key",
              "match": { "value": "R", "kind": "press" }, "cooldown": 0.2,
              "do": [ { "effect": "kill_radius", "radius": 220 } ] }
          ]
        }
        """;

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
        runtime.LoadReactions(SampleReactions);
        runtime.FeedFocus(true);
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

        if (@event is InputEventKey keyEvent &&
            (keyEvent.Keycode == Key.Space || keyEvent.Keycode == Key.E ||
             keyEvent.Keycode == Key.R))
        {
            Clay.ClayKey key = keyEvent.Keycode switch
            {
                Key.E => Clay.ClayKey.E,
                Key.R => Clay.ClayKey.R,
                _ => Clay.ClayKey.Space
            };
            runtime.FeedKeyAt(key, keyEvent.Pressed,
                              runtime.CursorX, runtime.CursorY);
        }
        else if (@event is InputEventMouseButton mouseEvent &&
                 mouseEvent.ButtonIndex == MouseButton.Left)
        {
            runtime.FeedKeyAt(Clay.ClayKey.MouseLeft, mouseEvent.Pressed,
                              mouseEvent.Position.X, mouseEvent.Position.Y);
        }
        else if (@event is InputEventMouseButton wheelEvent)
        {
            int wheel = wheelEvent.ButtonIndex switch
            {
                MouseButton.WheelUp => 1,
                MouseButton.WheelDown => -1,
                _ => 0
            };
            if (wheel != 0)
                runtime.FeedWheel(wheelEvent.Position.X, wheelEvent.Position.Y,
                                  wheel);
        }
        else if (@event is InputEventMouseMotion motionEvent)
        {
            runtime.FeedMotion(motionEvent.Position.X, motionEvent.Position.Y,
                               motionEvent.Relative.X, motionEvent.Relative.Y);
        }
    }

    public override void _Notification(int what)
    {
        if (runtime == null)
            return;

        if (what == NotificationApplicationFocusIn)
            runtime.FeedFocus(true);
        else if (what == NotificationApplicationFocusOut)
            runtime.FeedFocus(false);
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
