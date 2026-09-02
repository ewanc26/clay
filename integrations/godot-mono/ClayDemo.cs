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

        if (@event is InputEventJoypadButton joypadEvent)
        {
            Clay.ClayKey? key = ToClayKey(joypadEvent.ButtonIndex);
            if (key.HasValue)
                runtime.FeedKey(key.Value, joypadEvent.Pressed);
        }
        else if (@event is InputEventKey keyEvent)
        {
            Clay.ClayKey? key = ToClayKey(keyEvent.Keycode);
            if (key.HasValue)
                runtime.FeedKeyAt(key.Value, keyEvent.Pressed,
                                  runtime.CursorX, runtime.CursorY,
                                  ToClayModifiers(keyEvent));
        }
        else if (@event is InputEventMouseButton mouseEvent &&
                 ToClayKey(mouseEvent.ButtonIndex) is Clay.ClayKey mouseKey)
        {
            runtime.FeedKeyAt(mouseKey, mouseEvent.Pressed,
                              mouseEvent.Position.X, mouseEvent.Position.Y,
                              ToClayModifiers(mouseEvent));
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

    private static Clay.ClayKey? ToClayKey(JoyButton button) => button switch
    {
        JoyButton.A => Clay.ClayKey.GamepadA,
        JoyButton.B => Clay.ClayKey.GamepadB,
        JoyButton.X => Clay.ClayKey.GamepadX,
        JoyButton.Y => Clay.ClayKey.GamepadY,
        JoyButton.LeftShoulder => Clay.ClayKey.GamepadLeftBumper,
        JoyButton.RightShoulder => Clay.ClayKey.GamepadRightBumper,
        JoyButton.Back => Clay.ClayKey.GamepadBack,
        JoyButton.Start => Clay.ClayKey.GamepadStart,
        JoyButton.LeftStick => Clay.ClayKey.GamepadLeftStick,
        JoyButton.RightStick => Clay.ClayKey.GamepadRightStick,
        JoyButton.DpadUp => Clay.ClayKey.GamepadDpadUp,
        JoyButton.DpadDown => Clay.ClayKey.GamepadDpadDown,
        JoyButton.DpadLeft => Clay.ClayKey.GamepadDpadLeft,
        JoyButton.DpadRight => Clay.ClayKey.GamepadDpadRight,
        _ => null
    };

    private static Clay.ClayKey? ToClayKey(Key key)
    {
        int value = (int)key;
        if (value >= (int)Key.A && value <= (int)Key.Z)
            return Clay.ClayKey.A + (value - (int)Key.A);
        if (value >= (int)Key.Key0 && value <= (int)Key.Key9)
            return Clay.ClayKey.Digit0 + (value - (int)Key.Key0);

        return key switch
        {
            Key.Escape => Clay.ClayKey.Escape,
            Key.Enter => Clay.ClayKey.Enter,
            Key.Tab => Clay.ClayKey.Tab,
            Key.Space => Clay.ClayKey.Space,
            Key.Backspace => Clay.ClayKey.Backspace,
            Key.Delete => Clay.ClayKey.Delete,
            Key.Home => Clay.ClayKey.Home,
            Key.End => Clay.ClayKey.End,
            Key.Pageup => Clay.ClayKey.PageUp,
            Key.Pagedown => Clay.ClayKey.PageDown,
            Key.Up => Clay.ClayKey.ArrowUp,
            Key.Down => Clay.ClayKey.ArrowDown,
            Key.Left => Clay.ClayKey.ArrowLeft,
            Key.Right => Clay.ClayKey.ArrowRight,
            Key.F1 => Clay.ClayKey.F1,
            Key.F2 => Clay.ClayKey.F2,
            Key.F3 => Clay.ClayKey.F3,
            Key.F4 => Clay.ClayKey.F4,
            Key.F5 => Clay.ClayKey.F5,
            Key.F6 => Clay.ClayKey.F6,
            Key.F7 => Clay.ClayKey.F7,
            Key.F8 => Clay.ClayKey.F8,
            Key.F9 => Clay.ClayKey.F9,
            Key.F10 => Clay.ClayKey.F10,
            Key.F11 => Clay.ClayKey.F11,
            Key.F12 => Clay.ClayKey.F12,
            Key.Apostrophe => Clay.ClayKey.Quote,
            Key.Comma => Clay.ClayKey.Comma,
            Key.Period => Clay.ClayKey.Period,
            Key.Slash => Clay.ClayKey.Slash,
            Key.Semicolon => Clay.ClayKey.Semicolon,
            Key.Minus => Clay.ClayKey.Minus,
            Key.Equal => Clay.ClayKey.Equals,
            Key.Bracketleft => Clay.ClayKey.BracketLeft,
            Key.Bracketright => Clay.ClayKey.BracketRight,
            Key.Backslash => Clay.ClayKey.Backslash,
            Key.Quoteleft => Clay.ClayKey.Grave,
            Key.Shift => Clay.ClayKey.LeftShift,
            Key.Ctrl => Clay.ClayKey.LeftControl,
            Key.Alt => Clay.ClayKey.LeftAlt,
            Key.Meta => Clay.ClayKey.LeftMeta,
            _ => null
        };
    }

    private static Clay.ClayKey? ToClayKey(MouseButton button) => button switch
    {
        MouseButton.Left => Clay.ClayKey.MouseLeft,
        MouseButton.Right => Clay.ClayKey.MouseRight,
        MouseButton.Middle => Clay.ClayKey.MouseMiddle,
        MouseButton.Xbutton1 => Clay.ClayKey.MouseX1,
        MouseButton.Xbutton2 => Clay.ClayKey.MouseX2,
        _ => null
    };

    private static Clay.ClayModifiers ToClayModifiers(InputEventWithModifiers input)
    {
        Clay.ClayModifiers mods = Clay.ClayModifiers.None;
        if (input.ShiftPressed) mods |= Clay.ClayModifiers.Shift;
        if (input.CtrlPressed) mods |= Clay.ClayModifiers.Control;
        if (input.AltPressed) mods |= Clay.ClayModifiers.Alt;
        if (input.MetaPressed) mods |= Clay.ClayModifiers.Meta;
        return mods;
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
