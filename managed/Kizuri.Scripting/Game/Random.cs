// Random — utilidade de números aleatórios pro jogo. Puramente managed.
namespace Kizuri;

public static class Random
{
	private static readonly System.Random s_Rng = new System.Random();

	// Float em [min, max].
	public static float Float(float min = 0f, float max = 1f)
		=> min + (float)(s_Rng.NextDouble() * (max - min));

	// Int em [min, max) — min incluso, max excluso.
	public static int Int(int min, int max) => s_Rng.Next(min, max);

	// true com probabilidade p (0..1).
	public static bool Chance(float probability) => s_Rng.NextDouble() < probability;

	public static Math.Vector2 Vector2(float minX, float maxX, float minY, float maxY)
		=> new(Float(minX, maxX), Float(minY, maxY));

	public static Math.Vector3 Vector3(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
		=> new(Float(minX, maxX), Float(minY, maxY), Float(minZ, maxZ));
}
