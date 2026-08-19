
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

	
	
	public static float time => (float)Interop.KizuriNative.kz_time_get_time();
	public static float unscaledTime => (float)Interop.KizuriNative.kz_time_get_unscaled_time();
	public static float unscaledDeltaTime => (float)Interop.KizuriNative.kz_time_delta_seconds();

	
	public static ulong frameCount => Interop.KizuriNative.kz_time_get_frame();

	
	
	
	
	public static float TimeScale
	{
		get => Interop.KizuriNative.kz_get_time_scale();
		set => Interop.KizuriNative.kz_set_time_scale(value);
	}
}