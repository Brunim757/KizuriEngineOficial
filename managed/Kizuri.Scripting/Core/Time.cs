// Time — espelha kizuri/core/Timestep.hpp pro jogo.
namespace Kizuri;

public readonly struct Timestep
{
	public Timestep(float seconds) => Seconds = seconds;
	public float Seconds { get; }
	public float DeltaSeconds => Seconds;

	public static implicit operator float(Timestep ts) => ts.Seconds;
}

public static class Time
{
	public static float DeltaSeconds => (float)Interop.KizuriNative.kz_time_delta_seconds();
}