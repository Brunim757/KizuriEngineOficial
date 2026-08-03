// Program.cs — Main de aplicação exigida pelo OutputType Exe (que é o que
// faz o build gerar o SampleGame.runtimeconfig.json/.deps.json que o host
// CoreCLR da engine usa). Nenhuma lógica de jogo roda aqui: quem comanda o
// ciclo de vida é a engine (KizuriGame/editor), que chama os [GameEntryPoint]
// via Kizuri.Hosting.Host.
namespace SampleGame;

internal static class Program
{
	private static void Main()
	{
	}
}
