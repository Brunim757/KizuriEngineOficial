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

	// Para todos os sons em reprodução (usado por trocas de cena).
	public static void StopAll() => Interop.KizuriNative.kz_audio_stop_all();
}
