
namespace Kizuri;

public static class Mathf
{
	public static float Clamp(float value, float min, float max)
		=> value < min ? min : (value > max ? max : value);

	public static int Clamp(int value, int min, int max)
		=> value < min ? min : (value > max ? max : value);

	public static float Lerp(float a, float b, float t)
		=> a + (b - a) * Clamp(t, 0f, 1f);

	public static float LerpUnclamped(float a, float b, float t)
		=> a + (b - a) * t;

	
	public static float MoveTowards(float current, float target, float maxDelta)
	{
		if (System.MathF.Abs(target - current) <= maxDelta) return target;
		return current + System.MathF.Sign(target - current) * maxDelta;
	}

	
	
	public static float SmoothDamp(float current, float target, ref float velocity, float smoothTime, float deltaSeconds)
	{
		float omega = 2f / System.MathF.Max(smoothTime, 0.0001f);
		float x = omega * deltaSeconds;
		float exp = 1f / (1f + x + 0.48f * x * x + 0.235f * x * x * x);
		float change = current - target;
		float temp = (velocity + omega * change) * deltaSeconds;
		velocity = (velocity - omega * temp) * exp;
		return target + (change + temp) * exp;
	}

	
	public static float Repeat(float t, float length)
		=> Clamp(t - System.MathF.Floor(t / length) * length, 0f, length);

	
	public static float SmoothStep(float a, float b, float t)
	{
		t = Clamp((t - a) / (b - a), 0f, 1f);
		return t * t * (3f - 2f * t);
	}

	
	public static float PingPong(float t, float length)
	{
		t = Repeat(t, length * 2f);
		return length - System.MathF.Abs(t - length);
	}

	
	public static float Remap(float value, float fromMin, float fromMax, float toMin, float toMax)
		=> toMin + (value - fromMin) / (fromMax - fromMin) * (toMax - toMin);

	
	public static float LerpAngle(float a, float b, float t)
	{
		float delta = Repeat(b - a, 360f);
		if (delta > 180f) delta -= 360f;
		return a + delta * Clamp(t, 0f, 1f);
	}

	
	public static float Angle(Math.Vector2 a, Math.Vector2 b)
		=> System.MathF.Atan2(a.X * b.Y - a.Y * b.X, a.X * b.X + a.Y * b.Y) * 180f / System.MathF.PI;

	public static float Abs(float v) => System.MathF.Abs(v);
	public static float Sign(float v) => System.MathF.Sign(v);
	public static float Min(float a, float b) => System.MathF.Min(a, b);
	public static float Max(float a, float b) => System.MathF.Max(a, b);
	public static float Sqrt(float v) => System.MathF.Sqrt(v);
	public static float Pow(float v, float e) => System.MathF.Pow(v, e);
	public static float Sin(float v) => System.MathF.Sin(v);
	public static float Cos(float v) => System.MathF.Cos(v);

	
	public static float Distance(Math.Vector2 a, Math.Vector2 b)
		=> (a - b).Length;

	public static float Distance(Math.Vector3 a, Math.Vector3 b)
		=> (a - b).Length;
}
