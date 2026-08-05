// Audio — áudio global de jogo: sons one-shot (não ligados a entidade) e
// parar tudo de uma vez. Áudio ligado a entidade (posicional) se usa via
// Entity.AddAudio / Entity.PlayAudio (ver AudioSourceComponent na engine).
namespace Kizuri;

public static class Audio
{
	// Toca um arquivo de som uma vez, sem entidade (SFX avulso). O caminho é
	// relativo ao projeto/ao diretório de trabalho do jogo.
	public static void PlayOneShot(string clipPath, float volume = 1f)
		=> Interop.KizuriNative.kz_audio_play_one_shot(clipPath, volume);

	// One-shot POSICIONAL (3D): toca na posição do mundo, atenuado pela
	// distância ao ouvinte (a câmera). Ótimo pra impactos, passos, tiros.
	public static void PlayOneShotAt(string clipPath, float volume, Math.Vector3 position)
		=> Interop.KizuriNative.kz_audio_play_one_shot_at(clipPath, volume, position.X, position.Y, position.Z);

	// Para todos os sons em reprodução (usado por trocas de cena).
	public static void StopAll() => Interop.KizuriNative.kz_audio_stop_all();

	// Volume mestre global (0..1) de todos os sons.
	public static void SetMasterVolume(float volume) => Interop.KizuriNative.kz_audio_set_master_volume(volume);
}
