


namespace Kizuri;

public static class Rand
{
	private static readonly System.Random s_Rng = new System.Random();

	
	public static float Float(float min = 0f, float max = 1f)
		=> min + (float)(s_Rng.NextDouble() * (max - min));

	
	public static int Int(int min, int max) => s_Rng.Next(min, max);

	
	public static bool Chance(float probability) => s_Rng.NextDouble() < probability;

	public static Math.Vector2 Vector2(float minX, float maxX, float minY, float maxY)
		=> new(Float(minX, maxX), Float(minY, maxY));

	public static Math.Vector3 Vector3(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
		=> new(Float(minX, maxX), Float(minY, maxY), Float(minZ, maxZ));
}
