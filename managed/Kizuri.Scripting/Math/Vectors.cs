
namespace Kizuri.Math;

public struct Vector2
{
    public float X, Y;

    public Vector2(float x, float y) { X = x; Y = y; }

    public static Vector2 Zero => new(0f, 0f);
    public float Length => MathF.Sqrt(X * X + Y * Y);

    public static Vector2 operator +(Vector2 a, Vector2 b) => new(a.X + b.X, a.Y + b.Y);
    public static Vector2 operator -(Vector2 a, Vector2 b) => new(a.X - b.X, a.Y - b.Y);
    public static Vector2 operator *(Vector2 a, float s) => new(a.X * s, a.Y * s);
    public static Vector2 operator /(Vector2 a, float s) => new(a.X / s, a.Y / s);
}

public struct Vector3
{
    public float X, Y, Z;

    public Vector3(float x, float y, float z) { X = x; Y = y; Z = z; }
    public Vector3(float all) : this(all, all, all) { }
    public Vector3(Vector2 xy, float z) : this(xy.X, xy.Y, z) { }

    public static Vector3 Zero => new(0f, 0f, 0f);
    public static Vector3 One => new(1f, 1f, 1f);
    public float Length => MathF.Sqrt(X * X + Y * Y + Z * Z);

    public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vector3 operator -(Vector3 a, Vector3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vector3 operator *(Vector3 a, float s) => new(a.X * s, a.Y * s, a.Z * s);
}

public struct Vector4
{
    public float X, Y, Z, W;

    public Vector4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }
    public static Vector4 operator *(Vector4 a, float s) => new(a.X * s, a.Y * s, a.Z * s, a.W * s);
}