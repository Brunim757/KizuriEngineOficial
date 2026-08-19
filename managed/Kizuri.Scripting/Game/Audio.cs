


namespace Kizuri;

public static class Audio
{
	
	
	public static void PlayOneShot(string clipPath, float volume = 1f)
		=> Interop.KizuriNative.kz_audio_play_one_shot(clipPath, volume);

	
	
	public static void PlayOneShotAt(string clipPath, float volume, Math.Vector3 position)
		=> Interop.KizuriNative.kz_audio_play_one_shot_at(clipPath, volume, position.X, position.Y, position.Z);

	
	public static void StopAll() => Interop.KizuriNative.kz_audio_stop_all();

	
	public static void SetMasterVolume(float volume) => Interop.KizuriNative.kz_audio_set_master_volume(volume);

	
	public static float MusicVolume
	{
		get => Interop.KizuriNative.kz_audio_get_group_volume(1);
		set => Interop.KizuriNative.kz_audio_set_group_volume(1, value);
	}
	public static float SFXVolume
	{
		get => Interop.KizuriNative.kz_audio_get_group_volume(0);
		set => Interop.KizuriNative.kz_audio_set_group_volume(0, value);
	}
	public static float UIVolume
	{
		get => Interop.KizuriNative.kz_audio_get_group_volume(2);
		set => Interop.KizuriNative.kz_audio_set_group_volume(2, value);
	}

	
	public static void SetGlobalReverb(float wet, float roomSize = 0.5f, float damp = 0.3f)
		=> Interop.KizuriNative.kz_audio_set_global_reverb(wet, roomSize, damp);
}
