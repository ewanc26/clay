namespace Clay;

/// <summary>Modifier flags matching the public Clay C ABI.</summary>
[System.Flags]
public enum ClayModifiers
{
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Meta = 1 << 3,
}
