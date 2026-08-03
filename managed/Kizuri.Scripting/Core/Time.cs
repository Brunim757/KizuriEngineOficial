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

	// TimeScale global: 1.0 = normal, <1 = câmera lenta (0 = pausa). Escala o
	// deltaSeconds recebido pelos scripts (o OnUpdate de cada Script). A
	// física/partículas continuam com dt próprio — por isso 0 pausa os
	// scripts, não o mundo inteiro (v2 pode escalar tudo junto).
	public static float TimeScale
	{
		get => Interop.KizuriNative.kz_get_time_scale();
		set => Interop.KizuriNative.kz_set_time_scale(value);
	}
}